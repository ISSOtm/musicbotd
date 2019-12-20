
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <algorithm>
#include <array>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "client_connection.hpp"
#include "config_manager.hpp"
#include "server.hpp"


// The signals we stop the server on
static std::array const handledSignals = {SIGINT, SIGTERM};
static std::array<struct sigaction, handledSignals.size()> oldact;

static Server * serverInstance = nullptr;


static int const queue_length = 32;
static void tryConnectSocket(int & socket_fd, std::string const & port, struct addrinfo const * hints,
                             char const * protocol) {
    struct addrinfo * result;

    int gai_errno = getaddrinfo(NULL, port.c_str(), hints, &result);
    if (gai_errno) {
        spdlog::get("logger")->warn("Failure to init {} socket: {}", protocol,
                                    gai_strerror(gai_errno));
    } else {
        unsigned nbAttempts = 0;
        // We now have a list of possible addrinfo structs, try `bind`ing until one succeeds
        for (struct addrinfo * ptr = result; ptr; ptr = ptr->ai_next) {
            nbAttempts++;
            socket_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            // A failure is not a problem
            if (socket_fd == -1) {
                spdlog::get("logger")->debug("Attempt to create {} socket failed, trying next: {}",
                                             protocol, strerror(errno));
                continue;
            }
            // However, a success is a definitive success!
            if (bind(socket_fd, ptr->ai_addr, ptr->ai_addrlen) == 0
             && listen(socket_fd, queue_length) == 0) break;
            spdlog::get("logger")->debug("Attempt to open {} socket failed, trying next: {}",
                                         protocol, strerror(errno));
            close(socket_fd); // Clean up the socket we opened
            socket_fd = -1; // Revert back to failure state
        }
        freeaddrinfo(result);

        if (socket_fd == -1) {
            spdlog::get("logger")->warn("Failure to init {} socket : exhausted all {} options",
                                        protocol, nbAttempts);
        }
    }
}

Server::Server(ConfigManager & config)
 : _socket4_fd(-1), _socket6_fd(-1), _running(true), _nextConnectionID(0) {
    if (serverInstance) {
        // Running two server instances in the same process doesn't sound reasonable, so nothing
        // is designed to handle it.
        spdlog::get("logger")->critical("Running two server instances in the same process will misbehave!!");
    } else {
        // Register this server instance
        spdlog::get("logger")->trace("Registering signal handlers...");

        struct sigaction action;
        action.sa_handler = [](int){ serverInstance->stop(); };
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        for (unsigned i = 0; i < handledSignals.size(); i++) {
            sigaction(handledSignals[i], &action, &oldact[i]);
        }

        serverInstance = this;
    }

    std::string port = std::to_string(config.getInt("port"));
    spdlog::get("logger")->trace("Setting up connection on port {}...", port);

    // Try making an IPv4 socket
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    tryConnectSocket(_socket4_fd, port, &hints, "IPv4");

    // Now try again for IPv6
    hints.ai_family = AF_INET6;
    tryConnectSocket(_socket6_fd, port, &hints, "IPv6");

    // Now check if we have at *least* one socket open; otherwise, nothing we can do
    if (_socket4_fd == -1 && _socket6_fd == -1) {
        throw std::runtime_error("Could not open IPv4 or IPv6 socket");
    }
}

Server::~Server() {
    if (serverInstance == this) {
        spdlog::get("logger")->trace("Deregistering signal handlers...");

        for (unsigned i = 0; i < handledSignals.size(); i++) {
            sigaction(handledSignals[i], &oldact[i], NULL);
        }

        serverInstance = nullptr;
    }

    // Close the listening sockets
    spdlog::get("logger")->trace("Closing listening sockets...");
    if (_socket4_fd != -1) close(_socket4_fd);
    if (_socket6_fd != -1) close(_socket6_fd);

    spdlog::get("logger")->trace("~Server() done.");
}


void Server::run() {
    spdlog::get("logger")->trace("Setting up polling for sockets...");

    // The arguments to `ppoll`...
    struct timespec const timeout = { .tv_sec = 0, .tv_nsec = 100000 };
    std::array pollfds = {
        (struct pollfd){}, // The last two are to be filled for the IP sockets
        (struct pollfd){}
    };
    nfds_t nfds = pollfds.size() - 2;
    // To avoid a race condition, signals being caught need to be blocked during polling
    sigset_t blockedSignals;
    sigemptyset(&blockedSignals);
    for (auto const & signal : handledSignals) {
        sigaddset(&blockedSignals, signal);
    }

    // Set up 1 or 2 `pollfd`s depending on the available sockets
    auto setUpIPPoll = [&pollfds, &nfds](int const & socket) {
        if (socket == -1) return;
        pollfds[nfds].fd      = socket;
        pollfds[nfds].events  = POLLIN;
        ++nfds;
    };
    setUpIPPoll(_socket4_fd);
    setUpIPPoll(_socket6_fd);

    spdlog::get("logger")->info("Up and running!");


    while (_running) {
        switch (ppoll(pollfds.data(), nfds, &timeout, &blockedSignals)) {
            case -1:
                spdlog::get("logger")->error("server.run() ppoll() error: {}", strerror(errno));
                // fallthrough

            case 0: // Nothing to do
                break;

            default:
                // Check for any errors in file descriptors
                // I'm not sure how to handle them, but report them in the off chance it happens
                for (nfds_t i = 0; i < nfds; i++) {
                    if (pollfds[i].revents & POLLERR) {
                        spdlog::get("logger")->error("server.run(): file descr. {} [pollfd index {}] returned error", pollfds[i].fd, i);
                    }
                }
                // Check the listener sockets
                for (nfds_t i = pollfds.size() - 2; i < nfds; i++) {
                    if (pollfds[i].revents & POLLIN) {
                        handleNewConnection(pollfds[i].fd);
                    }
                }
        }

        // Check if any connections wish to die
        {
            std::lock_guard<std::mutex> lock(_closingReqMutex);
            while (!_closingRequests.empty()) {
                handleClosingConnection(_closingRequests.front());
                _closingRequests.pop();
            }
        }
    }

    spdlog::get("logger")->info("Cleaning up connections...");

    // Signal all connections they must terminate
    for (ClientConnection & connection : _connections) {
        connection.stop();
    }

    spdlog::get("logger")->trace("server.run() done.");
}

void Server::stop() {
    _running = false;
}


void Server::addWishToDie(ConnectionID id) {
    std::lock_guard<std::mutex> lock(_closingReqMutex);
    _closingRequests.push(id);
}


void Server::handleNewConnection(int socket) {
    struct sockaddr_storage addr;
    socklen_t addr_size = sizeof(addr);
    int new_socket = accept(socket, reinterpret_cast<struct sockaddr *>(&addr), &addr_size);
    if (new_socket == -1) {
        spdlog::get("logger")->error("accept() error: {}", strerror(errno));
        return;
    }

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in const * addrv4 = reinterpret_cast<struct sockaddr_in *>(&addr);
        uint32_t ip4 = ntohl(addrv4->sin_addr.s_addr);
        spdlog::get("logger")->info("Accepted connection {} from address {}.{}.{}.{}:{}", _nextConnectionID, ip4 >> 24, ip4 >> 16 & 255, ip4 >> 8 & 255, ip4 & 255, ntohs(addrv4->sin_port));
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6 const * addrv6 = reinterpret_cast<struct sockaddr_in6 *>(&addr);
        unsigned char const * ip6 = addrv6->sin6_addr.s6_addr;
        spdlog::get("logger")->info("Accepted connection {} from address {:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x} port {}", _nextConnectionID, ip6[0], ip6[1], ip6[2], ip6[3], ip6[4], ip6[5], ip6[6], ip6[7], ip6[8], ip6[9], ip6[10], ip6[11], ip6[12], ip6[13], ip6[14], ip6[15], ntohs(addrv6->sin6_port));
    } else {
        spdlog::get("logger")->warn("Accepted connection {} of unknown type {}", _nextConnectionID, addr.ss_family);
    }

    _connections.emplace(_connections.begin(), new_socket, *this, _nextConnectionID);
    ++_nextConnectionID;
}


void Server::handleClosingConnection(ConnectionID id) {
    _connections.remove_if([&id](ClientConnection const & connection) {
        return connection.id() == id;
    });
}

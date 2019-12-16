
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
        // We now have a list of possible addrinfo structs, try `bind`ing until one succeeds
        for (struct addrinfo * ptr = result; ptr; ptr = ptr->ai_next) {
            socket_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            // A failure is not a problem
            if (socket_fd == -1) continue;
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
            spdlog::get("logger")->warn("Failure to init {} socket : exhausted all options",
                                        protocol);
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
        spdlog::get("logger")->info("Registering signal handlers...");

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
    spdlog::get("logger")->info("Setting up connection on port {}...", port);

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
        spdlog::get("logger")->info("Deregistering signal handlers...");

        for (unsigned i = 0; i < handledSignals.size(); i++) {
            sigaction(handledSignals[i], &oldact[i], NULL);
        }

        serverInstance = nullptr;
    }

    // Close the listening sockets
    spdlog::get("logger")->info("Closing listening sockets...");
    if (_socket4_fd != -1) close(_socket4_fd);
    if (_socket6_fd != -1) close(_socket6_fd);
}


void Server::run() {
    spdlog::get("logger")->info("Setting up polling for sockets...");

    // The arguments to `ppoll`...
    struct timespec const timeout = { .tv_sec = 0, .tv_nsec = 1000 };
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

    while (!_connections.empty()) {
        _connections.remove_if([](ClientConnection const & connection) {
            return !connection.running();
        });
    }

    spdlog::get("logger")->info("Done.");
}

void Server::stop() {
    _running = false;
}


void Server::addWishToDie(ConnectionID id) {
    std::lock_guard<std::mutex> lock(_closingReqMutex);
    _closingRequests.push(id);
}


void Server::handleNewConnection(int socket) {
    struct sockaddr addr;
    socklen_t addr_size = sizeof(addr);
    int new_socket = accept(socket, &addr, &addr_size);
    if (new_socket == -1) {
        spdlog::get("logger")->error("accept() error: {}", strerror(errno));
        return;
    }

    if (socket == _socket4_fd) {
        uint32_t addrv4 = ((struct in_addr *)&addr)->s_addr;
        spdlog::get("logger")->info("Accepted connection from address {}.{}.{}.{}", addrv4 >> 24, addrv4 >> 16 & 255, addrv4 >> 8 & 255, addrv4 & 255);
    } else if (socket == _socket6_fd) {
        unsigned char * addrv6 = ((struct in6_addr *)&addr)->s6_addr;
        spdlog::get("logger")->info("Accepted connection from address {:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}", addrv6[0], addrv6[1], addrv6[2], addrv6[3], addrv6[4], addrv6[5], addrv6[6], addrv6[7], addrv6[8], addrv6[9], addrv6[10], addrv6[11], addrv6[12], addrv6[13], addrv6[14], addrv6[15], addrv6[16], addrv6[17], addrv6[18], addrv6[19], addrv6[20], addrv6[21], addrv6[22], addrv6[23], addrv6[24], addrv6[25], addrv6[26], addrv6[27], addrv6[28], addrv6[29], addrv6[30], addrv6[31]);
    } else {
        spdlog::get("logger")->warn("Accepted connection from unknown socket {} (IPv4 {}, IPv6 {})", socket, _socket4_fd, _socket6_fd);
    }

    _connections.emplace(_connections.begin(), socket, *this, _nextConnectionID);
    ++_nextConnectionID;
}


void Server::handleClosingConnection(ConnectionID id) {
    _connections.remove_if([&id](ClientConnection const & connection) {
        return connection.id() == id;
    });
}

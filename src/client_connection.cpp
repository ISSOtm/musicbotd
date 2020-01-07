
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <array>
#include <set>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>

#include "client_connection.hpp"


using namespace std::literals::chrono_literals;
std::chrono::steady_clock::duration const ClientConnection::timeout = 10s;
std::chrono::steady_clock::duration const ClientConnection::Conversation::timeout = 3s;


ClientConnection::ClientConnection(int socket, Server & server, Server::ConnectionID id)
 : _socket(socket), _server(server), _id(id),
   _pending(), _version(0), _conversations(), _lastActive(std::chrono::steady_clock::now()),
   _playlistName(""), _subscribed(false),
   _destructing(false), _running(true), _stopping(false), _thread([&](){run();}) {}

ClientConnection::~ClientConnection() {
    _destructing = true;

    spdlog::get("logger")->trace("Stopping connection {}...", _id);

    stop();
    _thread.join();

    close(_socket);
    unsubscribe();

    spdlog::get("logger")->trace("~ClientConnection({}) done.", _id);
}


void ClientConnection::stop() {
    _stopping = true;
}


void ClientConnection::run() {
    std::array pollfds{
        (struct pollfd){ .fd = _socket, .events = POLLIN | POLLRDHUP, .revents = 0 }
    };
    std::array<void (ClientConnection::*)(struct pollfd const &), pollfds.size()> handlers{
        &ClientConnection::handleConnection
    };
    while (!_stopping) {
        try {
            switch (poll(pollfds.data(), pollfds.size(), 100)) {
                case -1:
                    spdlog::get("logger")->error("ClientConnection[{}].run() poll() error: {}",
                                                 _id, strerror(errno));
                    // fallthrough

                case 0:
                    break;

                default:
                    for (unsigned i = 0; i < pollfds.size(); ++i) {
                        if (pollfds[i].revents & POLLERR) {
                            spdlog::get("logger")->error("ClientConnection[{}].run(): file descr {} [pollfd index {}] returned error", _id, pollfds[i].fd, i);
                        }
                        if (pollfds[i].revents & (pollfds[i].events | POLLHUP)) {
                            (this->*handlers[i])(pollfds[i]);
                        }
                    }
            }

            // Terminate conversations that have timed out
            auto iter = _conversations.cbegin();
            while (iter != _conversations.cend()) {
                auto cur = iter;
                ++iter; // Get the next one now, since erasing invalidates `cur`
                if (std::get<1>(*cur)->hasTimedOut()) {
                    spdlog::get("logger")->error("ClientConnection[{}] conv {} timed out",
                                                 _id, std::get<0>(*cur));
                    _conversations.erase(cur);
                }
            }

            // Terminate ourselves if we timed out
            if (hasTimedOut()) {
                stop();
            }

        } catch (std::exception const & e) {
            spdlog::get("logger")->error("ClientConnection[{}].run(): Exception at top level: {}", _id, e.what());
            stop();
        } catch (...) {
            spdlog::get("logger")->error("ClientConnection[{}].run(): Unknown exception at top level", _id);
            stop();
        }
    }
    _running = false;

    requestDestruction();
}

void ClientConnection::requestDestruction() {
    // Do not register for destruction if we're already destructing
    if (!_destructing) {
        _server.addWishToDie(_id);
    }
}

void ClientConnection::handleConnection(struct pollfd const & fd) {
    bool terminate = false; // Set this to request closing the connection

    if (fd.revents & POLLIN) { // Incoming data!
        std::stringstream s;
        std::array<char, BUFSIZ> buffer;
        ssize_t size = recv(_socket, buffer.data(), buffer.size(), 0);
        if (size == -1) {
            spdlog::get("logger")->error("ClientConnection[{}] recv() error: {}", _id,
                                         strerror(errno));
        } else if (size != 0) { // size == 0 happens when connection gets closed
            std::string_view readBuf(buffer.data(), std::min(size, (ssize_t)BUFSIZ));

            while (true) {
                auto offset = readBuf.find('\0');
                // Append fragment to previous fragment (if there's anything to append)
                if (offset) _pending.append(readBuf.data(), std::min(offset, readBuf.size()));
                // If no terminator was found, do not try to parse
                if (offset == readBuf.npos) break;
                // Skip the characters just read, plus the `\0`
                readBuf.remove_prefix(offset + 1);
                // Try deserializing the object
                try {
                    nlohmann::json packet = nlohmann::json::parse(_pending);
                    handlePacket(packet);
                } catch (nlohmann::json::parse_error const & e) {
                    spdlog::get("logger")->trace("ClientConnection[{}] recieved malformed JSON: {}", _id, e.what());
                }
                // Reset buffer
                _pending.clear();
            }
        }
    }

    if (fd.revents & (POLLRDHUP | POLLHUP)) {
        // Peer closed connection
        spdlog::get("logger")->info("ClientConnection[{}] closed by peer", _id);
        terminate = true;
    }

    if (terminate) {
        stop();
    }
}


void ClientConnection::sendPacket(nlohmann::json const & packet) {
    std::string const data = packet.dump();
    if (send(_socket, data.data(), data.size(), MSG_NOSIGNAL) == -1) {
        spdlog::get("logger")->error("ClientConnection[{}] send() error: {}", _id,
                                     strerror(errno));

        if (errno == EPIPE) {
            // If the connection suffers a broken pipe (= timeout), kill it
            stop();
        }
    }
}


// This class wraps a `map` around API version handlers
class APIMappings {
public:
    using Key         = unsigned;
    using ReturnType  = std::unique_ptr<ClientConnection::Conversation>;

private:
    std::map<Key, ReturnType(*)(ClientConnection & owner, int id)> _map;

public:
    template<typename... Ps>
    APIMappings(Ps&&... pairs) {
        insertAll(std::forward<Ps>(pairs)...);
    }

private:
    template<typename K, template<typename> class H, typename V, typename... Ps>
    void insertAll(K&& k, H<V>, Ps&&... ps) {
        _map.insert({std::forward<K>(k), [](ClientConnection & owner, int id){
            return ReturnType{std::make_unique<V>(owner, id)};
        }});
        insertAll(std::forward<Ps>(ps)...);
    }

    void insertAll() {}

public:
    decltype(auto) at(Key const & k) const { return _map.at(k); }
    decltype(_map)::const_iterator begin() const { return _map.begin(); }
    decltype(_map)::const_iterator end() const { return _map.end(); }
};

// Semi-useless type, only used to pass its parameter type
template<typename T> struct APIVersion {};

static APIMappings const packetHandlers{
    // Note: this should never have a key of "0"
    1, APIVersion<v1Conversation>{},
    // 2, APIVersion<v2Conversation>{},
    // etc
};

void ClientConnection::handlePacket(nlohmann::json const & packet) try {
    spdlog::get("logger")->trace("ClientConnection[{}] (version={}) recieved json {}",
                                 _id, _version, packet.dump());
    if (_version == 0) {
        // Negotiating API version
        if (!packet.is_array()) {
            spdlog::get("logger")->error("ClientConnection[{}] expected API version array, rejecting", _id);
            return;
        }
        handleNegotiation(packet);
        // If the negotiation failed, close the connection
        if (_version == 0) stop();

    } else {
        // Parse ID field to assign this to the correct conversation
        if (!packet.is_object()) {
            spdlog::get("logger")->error("ClientConnection[{}] expected object, rejecting",
                                         _id);
            return;
        }

        int id = packet["id"];
        try {
            if (id < 0) {
                // One-off message
                packetHandlers.at(_version)(*this, id)->handlePacket(packet);
            } else {
                // Check if the conversation exists
                auto conversation = _conversations.find(id);
                if (conversation == _conversations.end()) {
                    conversation = std::get<0>(_conversations.emplace(
                        std::piecewise_construct,
                        std::tuple(id), std::tuple(packetHandlers.at(_version)(*this, id))
                    ));
                }
                if (_conversations[id]->handlePacket(packet) == Conversation::Status::FINISHED) {
                    _conversations.erase(id);
                }
            }

            _lastActive = std::chrono::steady_clock::now();

        } catch (Conversation::StateMachineRejection const & e) {
            spdlog::get("logger")->error("ClientConnection[{}] conv {}: {}", _id, id, e.what());
        }
    }
} catch (nlohmann::json::exception const & e) {
    spdlog::get("logger")->error("ClientConnection[{}] got JSON error while using packet, rejecting: {}", _id, e.what());
}


static unsigned mostRecentSharedVersion(std::set<unsigned> set1, std::set<unsigned> set2) {
    auto it1 = set1.crbegin(), it2 = set2.crbegin();
    while (it1 != set1.crend() && it2 != set2.crend()) {
        if (*it1 == *it2) return *it1; // If this doesn't work, one of the numbers is too large
        ++(*it1 > *it2 ? it1 : it2); // If so, increment the problematic iterator
    }
    return 0; // Default value, will never appear in the handled version list
}

void ClientConnection::handleNegotiation(nlohmann::json const & packet) {
    // Construct the sorted set of supported API versions (from `packetHandlers`'s keys')
    std::set<unsigned> supported;
    for (auto const & [version, handler] : packetHandlers) {
        supported.insert(version);
    }

    // Construct the sorted set of requested API versions (from the JSON)
    std::set<unsigned> requested;
    for (auto const & version : packet) {
        if (!version.is_number_unsigned()) {
            spdlog::get("logger")->error("ClientConnection[{}] got malformed API array (expected only unsigned ints)");
            return;
        }
        requested.insert(version.get<unsigned>());
    }

    // Find the largest common element, and send that
    _version = mostRecentSharedVersion(supported, requested);
    spdlog::get("logger")->trace("ClientConnection[{}] selected version {}", _id, _version);
    sendPacket(nlohmann::json(_version)); // Sends a 0 in case of failure, notifying the client
}

void ClientConnection::subscribe() {
    if(!_subscribed) {
        _server.subscribe(_playlistName);
        _subscribed = true;
    }
}
void ClientConnection::unsubscribe() {
    if(_subscribed) {
        _server.unsubscribe(_playlistName);
        _subscribed = false;
    }
}
void ClientConnection::selectPlaylist(std::string const & name) {
    unsubscribe();
    _playlistName = name;
}


ClientConnection::Conversation::Conversation(ClientConnection & owner, int id)
 : _owner(owner), _lastActive(std::chrono::steady_clock::now()), _state(0), _id(id) {}

ClientConnection::Conversation::Status ClientConnection::Conversation::handlePacket(nlohmann::json const & packet) {
    Status status = _handlePacket(packet);
    // Refresh the timeout if the packet was correctly processed
    if (status == Status::CONTINUING) {
        _lastActive = std::chrono::steady_clock::now();
    }
    return status;
}

void ClientConnection::Conversation::sendPacket(nlohmann::json & json) {
    json["id"] = _id;
    _owner.sendPacket(json);
}

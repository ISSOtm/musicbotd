#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include <chrono>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "server.hpp"


struct pollfd; // Forward-declared because it's sufficient, over `#include <poll.h>`


// Manages a connection with a client, including the selected playlist, etc.
class ClientConnection {
public:
    // Manages a "conversation" of messages
    /* abstract */ class Conversation {
    public:
        static std::chrono::steady_clock::duration const timeout;

    public:
        enum class Status { FINISHED, CONTINUING, UNEXPECTED };

    private:
        std::chrono::steady_clock::time_point _lastActive;
        unsigned _state; // State internal to the type

    public:
        Conversation();
        virtual ~Conversation() = default;

        // Returns true if the packet processed was the last one, then the object is destroyed
        Status handlePacket(nlohmann::json const & packet);
        // This behavior is identical for *all* classes
        bool hasTimedOut() const;
    protected:
        // Utility function for children classes to call
        template<std::size_t size, typename T>
        Status processStateMachine(std::array<std::map<T, std::pair<Status, unsigned> (*)(nlohmann::json const &)>, size> const & transitions, T key, nlohmann::json const & packet);
    private:
        // Each implementation's own packet handling
        virtual Status _handlePacket(nlohmann::json const & packet) = 0;
    };


private:
    int _socket;
    Server & _server;
    Server::ConnectionID _id;

    std::string _pending; // Partial messages are stored here
    unsigned _version; // The version of the API used to dialog over this connection
    std::map<int, std::unique_ptr<Conversation>> _conversations;

    bool _destructing; // Set to true when the object is being destructed
    bool _running; // Set to false when the thread stops
    bool _stopping; // Set to true when the thread should stop (but it may still be running)
    // Must be last, so it gets initialized last
    std::thread _thread; // The thread managing the connection

public:
    ClientConnection(int socket, Server & server, Server::ConnectionID id);
    ~ClientConnection();

    Server::ConnectionID id() const { return _id; }
    void stop();
    bool running() const { return _running; }
private:
    void run();
    void requestDestruction();

    void handleConnection(struct pollfd const &);
    void handlePacket(nlohmann::json const & packet);
    void handleNegotiation(nlohmann::json const & packet);

    void sendPacket(nlohmann::json const & packet);
};


template<std::size_t size, typename T>
ClientConnection::Conversation::Status ClientConnection::Conversation::processStateMachine(std::array<std::map<T, std::pair<Status, unsigned> (*)(nlohmann::json const &)>, size> const & transitions, T key, nlohmann::json const & packet) {
    Status ret;
    std::tie(ret, _state) = transitions[_state].at(key)(packet);
    return ret;
}


// List of classes (extending `Conversation`) handling different API versions
#include "api/v1.hpp"


#endif

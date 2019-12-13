#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include <thread>

#include "server.hpp"


// Manages a connection with a client, including the selected playlist, etc.
class ClientConnection {
private:
    int _socket;
    Server & _server;
    Server::ConnectionID _id;

    bool _running; // Set to false when the thread stops
    bool _stopping; // Set to true when the thread should stop (but it may still be running)
    // Must be last, so it's initialized last
    std::thread _thread; // The thread managing the connection

public:
    ClientConnection(int socket, Server & server, Server::ConnectionID id);
    ~ClientConnection();

    Server::ConnectionID id() const;
    void stop();
    bool running() const;
private:
    void run();
};


#endif

#ifndef SERVER_HPP
#define SERVER_HPP

#include <mutex>
#include <list>
#include <queue>


class ClientConnection;

class Server {
public:
    // FIXME: Using a sequence of IDs is kinda sucky because there can be conflicts
    //        due to overflow; however, this is not a context in which it's important
    using ConnectionID = unsigned long long;

    static std::array<int, 2> const handledSignals;


private:
    int _socket4_fd; // File descriptor for the IPv4 socket
    int _socket6_fd; // File descriptor for the IPv6 socket

    bool _running; // Set to false when the server recieves SIGTERM

    ConnectionID _nextConnectionID; // The ID of the next connection to be generated
    std::list<ClientConnection> _connections;
    std::mutex _closingReqMutex; // Mutex for modifying what's below
    std::queue<ConnectionID> _closingRequests; // The IDs of the connections wishing to die

public:
    Server(char const * const port);
    ~Server();

    void run(); // Loops infinitely until stopped, handling incoming connections
    void stop(); // Signals the server to stop, but doesn't kill it immediately
    void addWishToDie(ConnectionID id); // Call to request a ClientConnection's destruction
private:
    void handleNewConnection(int socket); // Accepts a connection on the given socket
    void handleClosingConnection(ConnectionID id); // Destroy a connection object from its ID
};


#endif

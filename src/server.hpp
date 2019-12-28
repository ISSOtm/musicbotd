#ifndef SERVER_HPP
#define SERVER_HPP

#include <mutex>
#include <list>
#include <queue>
#include <thread>

#include "music/player.hpp"


class ClientConnection;
class ConfigManager;

struct addrinfo;

class Server {
public:
    // FIXME: Using a sequence of IDs is kinda sucky because there can be conflicts
    //        due to overflow; however, this is not a context in which it's important
    using ConnectionID = unsigned long long;


private:
    int _socket; // File descriptor for the listener socket

    bool _running; // Set to false when the server recieves SIGTERM

    Player _player;
    std::thread _playerThread;

    std::mutex _closingReqMutex; // Mutex for modifying what's below
    std::queue<ConnectionID> _closingRequests; // The IDs of the connections wishing to die
    ConnectionID _nextConnectionID; // The ID of the next connection to be generated
    std::list<ClientConnection> _connections;

    void tryConnectSocket(std::string const & port, struct addrinfo const * hints,
                          char const * protocol);
public:
    Server(ConfigManager & config);
    ~Server();

    void run(); // Loops infinitely until stopped, handling incoming connections
    void stop(); // Signals the server to stop, but doesn't kill it immediately
    void addWishToDie(ConnectionID id); // Call to request a ClientConnection's destruction
private:
    void handleNewConnection(int socket); // Accepts a connection on the given socket
    void handleClosingConnection(ConnectionID id); // Destroy a connection object from its ID

public:
    void play() { _player.play(); }
    void pause() { _player.pause(); }
    void appendMusic(Music const & music) { _player.appendMusic(music); }
};


#endif

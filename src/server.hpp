#ifndef SERVER_HPP
#define SERVER_HPP

#include <atomic>
#include <chrono>
#include <mutex>
#include <list>
#include <queue>
#include <thread>

#include "music/music_manager.hpp"
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

    std::atomic_bool _running; // Set to false when the server recieves SIGTERM

    std::mutex _closingReqMutex; // Mutex for modifying what's below
    std::queue<ConnectionID> _closingRequests; // The IDs of the connections wishing to die
    ConnectionID _nextConnectionID; // The ID of the next connection to be generated
    std::list<ClientConnection> _connections;

    MusicManager _manager;

    Player _player;
    std::thread _playerThread;

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
    bool playlistExists(std::string const & name) const { return _manager.playlistExists(name); }

    void addMusic(std::string const & playlist, Music const & music) {
        _manager.addMusic(playlist, music);
    }
    void appendMusic(Music const & music) { _player.appendMusic(music); }
    void newPlaylist(std::string const & name, std::string const & pass) {
        _manager.newPlaylist(name, pass);
    }
    void pause() { _player.pause(); }
    void play() { _player.play(); }
    void seek(double seconds) { _player.seek(seconds); }
    void subscribe(std::string const & name) { _manager.subscribe(name); }
    void unsubscribe(std::string const & name) { _manager.unsubscribe(name); }
};


#endif

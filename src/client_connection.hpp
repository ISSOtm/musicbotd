#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include <chrono>
#include <functional>
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

        enum class Status { FINISHED, CONTINUING, UNEXPECTED };

        class StateMachineRejection : public std::runtime_error {
        public:
            StateMachineRejection(unsigned state, unsigned key)
             : std::runtime_error("state " + std::to_string(state) + " got unexpected packet type " + std::to_string(key)) {}
        };

    protected:
        ClientConnection & _owner;
    private:
        std::chrono::steady_clock::time_point _lastActive;
        unsigned _state; // State internal to the type
        int _id;

    public:
        Conversation(ClientConnection & owner, int id);
        virtual ~Conversation() = default;

        // This behavior is identical for *all* classes
        bool hasTimedOut() const {
            return std::chrono::steady_clock::now() - _lastActive > timeout;
        };
        virtual void sendTimeout() = 0; // Called when the conversation times out
        // Returns true if the packet processed was the last one, then the object is destroyed
        Status handlePacket(nlohmann::json const & packet);

    protected: // This should be usable by implementors
        template<typename State>
        using TransitionFunc = std::function<std::pair<Status, State>(nlohmann::json const &)>;
        template<typename PacketType, typename State, std::size_t size>
        using TransitionMapping = std::array<std::map<PacketType, TransitionFunc<State>>, size>;

        // Utility function for children classes to call
        template<typename PacketType, typename State, std::size_t size>
        Status processStateMachine(TransitionMapping<PacketType, State, size> const & transitions, PacketType key, nlohmann::json const & packet) {
            TransitionFunc<State> transitionFunc;
            try {
                transitionFunc = transitions[_state].at(key);
            } catch (std::out_of_range const &) {
                throw StateMachineRejection(_state, static_cast<unsigned>(key));
            }
            Status ret;
            std::tie(ret, _state) = transitionFunc(packet);
            return ret;
        }
        void sendPacket(nlohmann::json & json);
    private:
        // Each implementation's own packet handling
        virtual Status _handlePacket(nlohmann::json const & packet) = 0;
    };


    static std::chrono::steady_clock::duration const timeout;


private:
    int _socket;
    Server & _server;
    Server::ConnectionID _id;

    std::string _pending; // Partial messages are stored here
    unsigned _version; // The version of the API used to dialog over this connection
    std::map<int, std::unique_ptr<Conversation>> _conversations;
    std::chrono::steady_clock::time_point _lastActive;

    std::string _playlistName;
    bool _subscribed;

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
    bool hasTimedOut() const { return std::chrono::steady_clock::now() - _lastActive > timeout; }

    void sendPacket(nlohmann::json const & packet);

    // Methods called by the `Conversation`s
public:
    bool subscribed() const { return _subscribed; }
    bool playlistExists(std::string const & name) const { return _server.playlistExists(name); }

    void addMusic(Music const & music) { _server.addMusic(_playlistName, music); }
    void appendMusic(Music const & music) { _server.appendMusic(music); }
    void newPlaylist(std::string const & name, std::string const & pass) {
        _server.newPlaylist(name, pass);
    }
    void pause() { _server.pause(); }
    void play() { _server.play(); }
    void seek(double seconds) { _server.seek(seconds); }
    void selectPlaylist(std::string const & name);
    void subscribe();
    void unsubscribe();
};


// List of classes (extending `Conversation`) handling different API versions
#include "api/v1.hpp"


#endif

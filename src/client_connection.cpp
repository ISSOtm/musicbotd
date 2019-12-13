
#include <unistd.h>

#include "client_connection.hpp"


ClientConnection::ClientConnection(int socket, Server & server, Server::ConnectionID id)
 : _socket(socket), _server(server), _id(id), _running(true), _stopping(false), _thread([&](){run();}) {
    _thread.detach();
}

ClientConnection::~ClientConnection() {
    close(_socket);
}


Server::ConnectionID ClientConnection::id() const {
    return _id;
}

bool ClientConnection::running() const {
    return _running;
}

void ClientConnection::stop() {
    _stopping = true;
}


void ClientConnection::run() {
    while (!_stopping) {
        // TODO
    }
    _running = false;
}


#include <unistd.h>

#include <spdlog/spdlog.h>

#include "client_connection.hpp"


ClientConnection::ClientConnection(int socket, Server & server, Server::ConnectionID id)
 : _socket(socket), _server(server), _id(id), _running(true), _stopping(false),
   _thread([&](){run();}) {
}

ClientConnection::~ClientConnection() {
    spdlog::get("logger")->trace("Stopping connection {}...", _id);

    stop();
    _thread.join();

    close(_socket);

    spdlog::get("logger")->trace("~ClientConnection({}) done.", _id);
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

    _server.addWishToDie(_id);
}

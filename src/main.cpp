
#include <csignal>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "server.hpp"


std::unique_ptr<Server> server; // The server instance

int main() {
    // First thing: init logging

    // Create a logger object; don't care about what it returns, it will always be retrieved
    // from spdlog's logger pool
    spdlog::stderr_color_mt("logger")->set_level(spdlog::level::trace);

    // Now, create the server...
    server = std::make_unique<Server>("1939");

    spdlog::get("logger")->info("Installing signal handlers...");

    struct sigaction action = {};
    action.sa_handler = [](int){ server->stop(); };
    for (int signal : Server::handledSignals) {
        sigaction(signal, &action, NULL);
    }

    // ...and run it!
    server->run();

    return 0;
}


#include <csignal>
#include <memory>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "server.hpp"


std::unique_ptr<Server> server; // The server instance

int main() {
    // First thing: init logging

    // Create a logger object; don't care about what it returns, it will always be retrieved
    // from spdlog's logger pool
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    console_sink->set_level(spdlog::level::warn);
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("musicbotd.log");
    file_sink->set_level(spdlog::level::trace);
    spdlog::register_logger(std::make_shared<spdlog::logger, std::string, spdlog::sinks_init_list>("logger", {console_sink, file_sink}));
    spdlog::set_pattern("[%Y-%m-%dT%T.%e] [%^%l%$] %v");

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

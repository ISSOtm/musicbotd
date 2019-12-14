
#include <memory>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "server.hpp"



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

    try {
        // Now, create the server, and run it!
        Server("1939").run();

        return 0;
    } catch (std::exception const & exception) {
        spdlog::get("logger")->critical("Exception at top level: {}", exception.what());
    } catch (...) {
        spdlog::get("logger")->critical("Unknown exception at top level, aborting");
    }

    return 1;
}


#include <memory>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "config_manager.hpp"
#include "server.hpp"



// ***WARNING***
// If `%t` is used while `SPDLOG_NO_THREAD_ID` is defined, behavior is undefined
// If `%n` is used while `SPDLOG_NO_NAME` is defined, behavior is undefined
// Remove variables from compilation flags to use those again
static char const * const log_format = "[%Y-%m-%dT%T.%e] [%^%l%$] %v";
int main() {
    // First thing: init logging
    // Logging to a file will not be done yet, since we need to read config for that
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    console_sink->set_pattern(log_format);
    // Create a logger object and register it into spdlog's global logger pool
    spdlog::register_logger(std::make_shared<spdlog::logger, std::string, spdlog::sink_ptr>("logger", console_sink));
    spdlog::get("logger")->set_level(spdlog::level::info);

    // Second thing: read config
    ConfigManager config("/etc/musicbotd.ini", "musicbotd.ini");

    // Now we can start logging to a file
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.getStr("log_file"));
    file_sink->set_level(spdlog::level::trace);
    file_sink->set_pattern(log_format);
    spdlog::get("logger")->sinks().push_back(file_sink);

    try {
        // Now, create the server, and run it!
        Server(config).run();

        return 0;
    } catch (std::exception const & exception) {
        spdlog::get("logger")->critical("Exception at top level: {}", exception.what());
    } catch (...) {
        spdlog::get("logger")->critical("Unknown exception at top level, aborting");
    }

    return 1;
}

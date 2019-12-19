
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
    // Create a logger object and register it into spdlog's global logger pool
    spdlog::stderr_color_mt("logger"); // Log to stderr because systemd will handle everything
    spdlog::get("logger")->set_level(spdlog::level::trace);
    spdlog::get("logger")->set_pattern(log_format);

    // Second thing: read config
    ConfigManager config("/etc/musicbotd.ini", "musicbotd.ini");

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

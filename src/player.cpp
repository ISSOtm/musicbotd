
#include <array>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "player.hpp"


double const Player::timeout = 0.1;


Player::Player()
 : _running(true), _mpv(mpv_create()) {
    if (_mpv == nullptr) {
        throw std::runtime_error("Failed to create MPV handle (does LC_NUMERIC != \"C\"?)");
    }

    spdlog::get("logger")->trace("Configuring and init-ing MPV player...");

    // Set config options
    mpv_set_option_string(_mpv, "load-scripts", "no"); // Don't load config scripts
    mpv_set_option_string(_mpv, "vid", "no"); // Disable video playback, we're only doing audio!

    int retcode = mpv_initialize(_mpv);
    if (retcode < 0) {
        throw std::runtime_error("Failed to initialize MPV: " + std::string(mpv_error_string(retcode)));
    }
}

Player::~Player() {
    spdlog::get("logger")->trace("Destroying MPV handle...");
    mpv_destroy(_mpv);
}


void Player::run() {
    spdlog::get("logger")->trace("MPV player up and running!");

    while (_running) {
        mpv_event const * event = mpv_wait_event(_mpv, Player::timeout);
        if (event->event_id != MPV_EVENT_NONE) {
            spdlog::get("logger")->trace("mpv event: {}", mpv_event_name(event->event_id));
        }
        switch (event->event_id) {
            case MPV_EVENT_NONE:
                // Do nothing
                break;

            case MPV_EVENT_SHUTDOWN:
                _running = false;
                break;
        }
    }

    spdlog::get("logger")->trace("MPV player finished running");
}

void Player::stop() {
    _running = false;
}


void Player::play() {
    spdlog::get("logger")->trace("Unpausing MPV player");
    setProperty("pause", false);
}

void Player::pause() {
    spdlog::get("logger")->trace("Pausing MPV player");
    setProperty("pause", true);
}

void Player::appendMusic(std::string const & url) {
    spdlog::get("logger")->trace("Queuing {}", url);
    runCommand("loadfile", url.c_str(), "append-play" /* TODO: pass options, such as `start` and `stop` */);
}

void Player::next() {
    runCommand("playlist-next", "force");
}

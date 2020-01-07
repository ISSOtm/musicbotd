
#include <cstring>
#include <array>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "player.hpp"



Player::Player()
 : _running(true), _mpv(mpv_create()) {
    if (_mpv == nullptr) {
        throw std::runtime_error("Failed to create MPV handle (does LC_NUMERIC != \"C\"?)");
    }

    spdlog::get("logger")->trace("Configuring and init-ing MPV player...");

    // Set config options
    mpv_set_option_string(_mpv, "cache", "yes"); // Enable caching since all comes from the net
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

            case MPV_EVENT_END_FILE:
                runCommand("playlist-remove", "0");
                break;
        }
    }

    spdlog::get("logger")->trace("MPV player finished running");
}

void Player::stop() {
    _running = false;
}


void Player::appendMusic(Music const & music) {
    std::string const & url = music.url();
    spdlog::get("logger")->trace("Queuing {}", url);
    runCommand("loadfile", url.c_str(), "append-play", music.options().c_str());
}

void Player::next() {
    runCommand("playlist-next", "force");
}

void Player::pause() {
    spdlog::get("logger")->trace("Pausing MPV player");
    setProperty("pause", true);
}

void Player::play() {
    spdlog::get("logger")->trace("Unpausing MPV player");
    setProperty("pause", false);
}

void Player::seek(double seconds) {
    runCommand("seek", std::to_string(seconds).c_str(), "absolute");
}


nlohmann::json Player::status() const {
    nlohmann::json packet{
        {"duration", getProperty<double>("duration")},
        {"pause",    getProperty<bool>("pause")},
        {"playlist", nlohmann::json::array()},
        {"position", getProperty<double>("playback-time")}
    };

    // TODO: check the type of the union
    mpv_node playlist = getProperty<mpv_node>("playlist");
    for (int i = 0; i < playlist.u.list->num; i++) {
        mpv_node const & node = playlist.u.list->values[i];
        for (int j = 0; j < node.u.list->num; j++) {
            char const * key       = node.u.list->keys[j];
            mpv_node const & value = node.u.list->values[j];

            if (!strcmp("filename", key)) {
                packet["playlist"][i] = value.u.string;
            } else if (!strcmp("title", key)) {
                packet["playlist"][i] = value.u.string;
                break; // This one has precedence over `filename`
            }
        }
    }
    mpv_free_node_contents(&playlist);

    return packet;
}

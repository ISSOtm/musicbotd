#ifndef PLAYER_HPP
#define PLAYER_HPP


#include <mpv/client.h>

#include <atomic>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

#include "music.hpp"


template<typename>
struct Format{};
template<>
struct Format<bool> {
    static constexpr mpv_format format = MPV_FORMAT_FLAG;
    using type = int;
};
template<>
struct Format<int64_t> {
    static constexpr mpv_format format = MPV_FORMAT_INT64;
    using type = int64_t;
};
template<>
struct Format<double> {
    static constexpr mpv_format format = MPV_FORMAT_DOUBLE;
    using type = double;
};
template<>
struct Format<mpv_node> {
    static constexpr mpv_format format = MPV_FORMAT_NODE;
    using type = mpv_node;
};

class Player {
public:
    static double constexpr timeout = 0.1;

private:
    std::atomic_bool _running;

    mpv_handle * _mpv;

    template<typename T, typename... Ts>
    int runCommand(T&& name, Ts&&... args) {
        char const * command[] = {
            std::forward<T>(name), std::forward<Ts>(args)..., nullptr
        };
        int retcode = mpv_command(_mpv, command);
        if (retcode < 0) {
            spdlog::get("logger")->error("Error while running MPV command " + std::string(name) + ": " + mpv_error_string(retcode));
        }
        return retcode;
    }

    template<typename T>
    T getProperty(char const * name) const {
        typename Format<T>::type data;
        int retcode = mpv_get_property(_mpv, name, Format<T>::format, &data);
        if (retcode < 0) {
            spdlog::get("logger")->error("Error getting MPV property " + std::string(name) + ": " + mpv_error_string(retcode));
            // TODO: still returning `data`. This sucks. Make a `MPVError` class and throw it
        }
        return data;
    }

    template<typename T>
    int setProperty(char const * name, T&& data) {
        typename Format<T>::type mpvData = std::forward<T>(data);
        int retcode = mpv_set_property(_mpv, name, Format<T>::format, &mpvData);
        if (retcode < 0) {
            spdlog::get("logger")->error("Error setting MPV property " + std::string(name) + ": " + mpv_error_string(retcode));
        }
        return retcode;
    }


public:
    Player();
    ~Player();
    void run();
    void stop();

    void appendMusic(Music const & music);
    void next();
    void pause();
    void play();
    void seek(double seconds);

    unsigned playlistSize() const;
    nlohmann::json status() const;
};


#endif

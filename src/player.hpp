#ifndef PLAYER_HPP
#define PLAYER_HPP


#include <mpv/client.h>

#include <spdlog/spdlog.h>
#include <string>


template<typename>
inline mpv_format getFormat() = delete;
template<>
inline mpv_format getFormat<bool>() { return MPV_FORMAT_FLAG; }
template<>
inline mpv_format getFormat<int64_t>() { return MPV_FORMAT_INT64; }
template<>
inline mpv_format getFormat<double>() { return MPV_FORMAT_DOUBLE; }

class Player {
public:
    static double const timeout;

private:
    bool _running;

    mpv_handle * _mpv;
    enum { INITED, IDLE, PLAYING, PAUSED } _mpvState;

    template<typename T, typename... Ts>
    int runCommand(T&& name, Ts&&... args) {
        int retcode = mpv_command(_mpv, (char const*[]){
            std::forward<T>(name), std::forward<Ts>(args)..., nullptr
        });
        if (retcode < 0) {
            spdlog::get("logger")->error("Error while running MPV command " + std::string(name) + ": " + mpv_error_string(retcode));
        }
        return retcode;
    }

    template<typename T>
    int setProperty(char const * name, T data) {
        int retcode = mpv_set_property(_mpv, name, getFormat<T>(), &data);
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

    void play();
    void pause();
    void appendMusic(std::string const & url /* TODO: params */);
    void next();
};


#endif

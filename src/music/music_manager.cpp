
#include <random>
#include <spdlog/spdlog.h>

#include "music_manager.hpp"


MusicManager::MusicManager()
 : _musics(),
   _playlists({decltype(_playlists)::value_type(std::piecewise_construct, std::tuple(""), std::tuple(""))}),
   _global_list(_playlists.begin()), _next(_playlists.begin()),
   _thread() {
    // TODO: read music list and playlists
}

Music const & MusicManager::nextMusic() {
    spdlog::get("logger")->trace("Trying to add new music...");
    std::lock_guard lock(_mutex);

    // First, find a playlist that's ready to add from
    bool looped = false;
    do {
        if (_next == _playlists.end()) {
            if (looped) { throw Playlist::NoMoreMusic(); } // Give up if we already looped
            // This cannot be the end because there is always at least the global playlist
            _next = _playlists.begin();
            looped = true;
        }
    } while (!std::get<1>(*_next).isSubscribed());

    return _musics.at(std::get<1>(*_next).nextMusic());
}


void MusicManager::addMusic(std::string const & playlist, Music const & music) {
    spdlog::get("logger")->trace("Adding \"{}\" to \"{}\"", music.url(), playlist);
    {
        std::lock_guard lock(_mutex);

        typename decltype(_musics)::const_iterator iter =
            std::find_if(_musics.begin(), _musics.end(),
                         [&music](typename decltype(_musics)::value_type const & pair) {
                           return std::get<1>(pair) == music;
            });

        if (iter == _musics.end()) {
            std::minstd_rand rng;
            bool inserted;
            do {
                std::tie(iter, inserted) = _musics.emplace(std::uniform_int_distribution<ID>()(rng),
                                                           music);
            } while (!inserted);
        }

        // Now add that Music to the playlist
        ID id = std::get<0>(*iter);
        _playlists.at(playlist).addMusic(id);
        if (!playlist.empty()) _playlists.at("").addMusic(id);
    }
}

void MusicManager::subscribe(std::string const & playlist) {
    spdlog::get("logger")->trace("Subscribed to playlist \"{}\"", playlist);
    std::lock_guard lock(_mutex);
    _playlists.at(playlist).subscribe();
}

void MusicManager::unsubscribe(std::string const & playlist) {
    spdlog::get("logger")->trace("Unsubscribed from playlist \"{}\"", playlist);
    std::lock_guard lock(_mutex);
    _playlists.at(playlist).unsubscribe();
}


#include "music_manager.hpp"


MusicManager::MusicManager()
 : _musics(),
   _playlists({decltype(_playlists)::value_type(std::piecewise_construct, std::tuple(""), std::tuple(""))}),
   _global_list(_playlists.begin()), _next(_playlists.begin()),
   _thread() {
    // TODO: read music list and playlists
}

Music const & MusicManager::nextMusic() {
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
    std::lock_guard lock(_mutex);

    typename decltype(_musics)::const_iterator iter;

    if (!playlist.empty()) {
        addMusic("", music); // Add to the global playlist as well
    } else {
        iter = std::find_if(_musics.begin(), _musics.end(),
                            [&music](typename decltype(_musics)::value_type const & pair) {
            return std::get<1>(pair) == music;
        });
    }
}
void MusicManager::subscribe(std::string const & playlist) {
    std::lock_guard lock(_mutex);
    _playlists.at(playlist).subscribe();
}

void MusicManager::unsubscribe(std::string const & playlist) {
    std::lock_guard lock(_mutex);
    _playlists.at(playlist).unsubscribe();
}
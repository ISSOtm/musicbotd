#ifndef MUSIC_MANAGER_HPP
#define MUSIC_MANAGER_HPP

#include <algorithm>
#include <map>
#include <string>
#include <thread>

#include "music.hpp"
#include "playlist.hpp"


class MusicManager {
public:
    using ID = uint64_t;
    using Playlist = Playlist<ID>;

private:
    std::map<ID, Music> _musics;

    std::map<std::string, Playlist> _playlists;
    decltype(_playlists)::const_iterator const _global_list;
    decltype(_playlists)::iterator _next; // Next candidate for music addition

    std::thread _thread;

public:
    MusicManager();

    Playlist const & playlist(std::string const & name) const { return _playlists.at(name); }
    Music const & nextMusic();

    void addMusic(std::string const & name, Music const & music);
    void subscribe(std::string const & name) { _playlists.at(name).subscribe(); }
    void unsubscribe(std::string const & name) { _playlists.at(name).unsubscribe(); }
};


#endif

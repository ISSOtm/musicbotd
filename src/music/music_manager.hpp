#ifndef MUSIC_MANAGER_HPP
#define MUSIC_MANAGER_HPP

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "music.hpp"
#include "playlist.hpp"


class MusicManager {
public:
    using ID = uint32_t;
    using NoMoreMusic = Playlist<ID>::NoMoreMusic;

private:
    std::map<ID, Music> _musics;

    std::map<std::string, Playlist<ID>> _playlists;
    decltype(_playlists)::const_iterator const _global_list;
    decltype(_playlists)::iterator _next; // Next candidate for music addition

    mutable std::mutex _mutex;

    std::thread _thread;

public:
    MusicManager();

    Music const & nextMusic();

    bool playlistExists(std::string const & name) const;

    Music getMusic(ID const & music) const;
    void addMusic(std::string const & name, Music const & music);
    void newPlaylist(std::string const & name, std::string const & pass);
    void subscribe(std::string const & name);
    void unsubscribe(std::string const & name);
};


#endif

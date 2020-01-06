#ifndef PLAYLIST_HPP
#define PLAYLIST_HPP

#include <algorithm>
#include <random>
#include <string>
#include <vector>


template<typename ID>
class Playlist {
public:
    class NoMoreMusic {};

private:
    std::string _password;

    std::vector<ID> _musics;
    std::minstd_rand _rng;
    int _remaining; // How many musics until re-shuffle

    unsigned _subscribers;


public:
    Playlist(std::string const & password)
     : _password(password), _remaining(0), _subscribers(0) {}

    bool checkPassword(std::string const & password) { return password == _password; }
    bool empty() const { return _musics.empty(); }
    bool isSubscribed() const { return  _subscribers && !empty(); }

    ID const & nextMusic() {
        if (empty()) { throw NoMoreMusic(); }

        // If all musics have played, refresh the pool
        if (_remaining == 0) _remaining = _musics.size();

        // Pick a music at random
        --_remaining;
        ID & selected = _musics[std::uniform_int_distribution(0, _remaining)(_rng)];
        std::swap(_musics[_remaining], selected);
        return selected;
    }
    void subscribe() {
        ++_subscribers;
    }
    void unsubscribe() {
        --_subscribers;
    }
};


#endif

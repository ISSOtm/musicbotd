#ifndef MUSIC_HPP
#define MUSIC_HPP

#include <map>
#include <string>


class Music {
private:
    std::string _url;
    std::map<std::string, std::string> _options;

public:
    Music(std::string const & url);
    void setOption(std::string const & key, std::string const & value);
    void unsetOption(std::string const & key);

    std::string const & url() const { return _url; }
    std::string options() const;

    bool operator==(Music const & music) const;
};


#endif

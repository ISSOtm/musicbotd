
#include <set>

#include "music.hpp"


Music::Music(std::string const & url)
 : _url(url) {}


static std::set<std::string> const allowedProperties{
    "start", "stop"
};

void Music::setOption(std::string const & key, std::string const & value) {
    // First, validate the key
    if (allowedProperties.find(key) == allowedProperties.cend()) {
        throw std::out_of_range("Setting property " + key + " is not allowed.");
    }

    // Then, set the option
    _options.insert_or_assign(key, value);
}

void Music::unsetOption(std::string const & key) {
    auto option = _options.find(key);
    // An option that doesn't exist is already unset
    if (option != _options.end()) {
        _options.erase(option);
    }
}


std::string Music::options() const {
    std::string options;
    for (auto [key, value] : _options) {
        if (!options.empty()) options += ",";
        options += key + "=" + value;
    }
    return options;
}


bool Music::operator==(Music const & music) const {
    return _url == music._url && _options == music._options;
}


#include <iostream>

#include "config_manager.hpp"


char const * const ConfigManager::separators = "= \t";
char const * const ConfigManager::whitespace = ConfigManager::separators + 1;


int const & ConfigManager::getInt(std::string const & key) const {
    try {
        return _properties.at(key)->getInt();
    } catch (Property::InvalidType const &) {
        throw std::runtime_error("Property \"" + key + "\" cannot be retrieved as int");
    }
}
std::string const & ConfigManager::getStr(std::string const & key) const {
    try {
        return _properties.at(key)->getStr();
    } catch (Property::InvalidType const &) {
        throw std::runtime_error("Property \"" + key + "\" cannot be retrieved as string");
    }
}


#include <iostream>

#include "config_manager.hpp"


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


std::map<int, void (ConfigManager::*)(std::ifstream &, std::locale const &)> ConfigManager::_parsers{
    {1, &ConfigManager::parse_v1}
};

void ConfigManager::parse_v1(std::ifstream & configFile, std::locale const & locale) {
    // FIXME: this needs a custom parser: the whitespace skipping also skips newlines...
    while (!configFile.eof()) {
        char firstChar;
        configFile >> firstChar;
        if (configFile.eof()) break;
        if (firstChar == ';')  { // Skip comment lines
            configFile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        configFile.unget(); // Push back the first char if this wasn't a comment line

        // Now read the property name
        std::array<char, 4096> rawPropName;
        // This cannot EOF because we pushed back a char with `unget` above
        configFile.getline(rawPropName.data(), rawPropName.size(), '=');
        if (configFile.eof()) {
            throw std::runtime_error(_path + ": Found property name '" + std::string(rawPropName.data()) + "' but no value");
        }
        configFile.ignore(1); // Ignore the `=` sign
        // Trim the end of the string
        std::string_view propName(rawPropName.data()); // Will scan the end of the string for us
        rawPropName[std::use_facet<std::ctype<char>>(locale).scan_is(std::ctype_base::space, &propName.front(), &propName.back() + 1) - rawPropName.data()] = '\0';

        // And read the property value
        std::string value;
        configFile >> value; // FIXME: this breaks with empty properties

        try {
            _properties.at(std::string(rawPropName.data()))->set(value);
        } catch (std::out_of_range const &) {
            throw std::runtime_error(_path + ": Unknown property '" + std::string(rawPropName.data()) + "'");
        }
    }
}

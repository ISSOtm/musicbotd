#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP


#include <exception>
#include <fstream>
#include <map>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>




class Property {
public:
    class InvalidType : public std::runtime_error {
    public:
        InvalidType() : std::runtime_error("Property isn't of specified type") {}
    };

    virtual ~Property() = default;

    virtual int const & getInt() const { throw InvalidType(); };
    virtual std::string const & getStr() const { throw InvalidType(); };

    virtual void set(std::string const & value) = 0;
};

template<int base>
class IntProperty : public Property {
    int _prop;
public:
    IntProperty(int const & value) : _prop(value) {}
    int const & getInt() const { return _prop; }
    void set(std::string const & value) { _prop = std::stoul(value, 0, base); }
};

class StringProperty : public Property {
    std::string _prop;
public:
    StringProperty(std::string const & value) : _prop(value) {}
    std::string const & getStr() const { return _prop; }
    void set(std::string const & value) { _prop = value; }
};


class ConfigManager {
private:
    static char const * const separators;
    static char const * const whitespace;
private:
    std::string _path;

    std::map<std::string, std::unique_ptr<Property>> _properties;

public:
    template<typename... T>
    ConfigManager(T const & ... paths);

    int const & getInt(std::string const & key) const;
    std::string const & getStr(std::string const & key) const;
};


template<typename... T>
ConfigManager::ConfigManager(T const & ... paths) {
    // Insert default values
    // Can't do this from std::map init because it insists on using a copy constructor
    _properties.emplace("port",     std::make_unique<IntProperty<10>>(1939));
    _properties.emplace("log_file", std::make_unique<StringProperty> ("musicbotd.log"));

    // Try opening all INI files, grabbing the first matching one (the most specific)
    std::ifstream configFile;

    for (std::string const & path : { paths... }) {
        configFile = std::ifstream(path);

        if (!configFile.fail()) {
            _path = path;
            goto found; // C++ does not have `for {} else {}`, sadly...
        }

        spdlog::get("logger")->warn("Failed to open ini file {}", path);
    }
    throw std::runtime_error("Failed to open any ini file");

found:
    // Read the config from the file
    unsigned lineNo = 0;
    std::array<char, 4096> lineArray;
    while (!configFile.eof()) {
        configFile.getline(lineArray.data(), lineArray.size());
        ++lineNo;

        if (configFile.fail() && !configFile.eof()) {
            throw std::runtime_error(_path + " line " + std::to_string(lineNo) + " is too large (" + std::to_string(lineArray.size() - 1) + " chars max)");
        }

        // Extract the property name and value (ignoring whitespace)
        std::string_view line = lineArray.data();
        // Skip leading whitespace
        auto initialSkip = line.find_first_not_of(ConfigManager::whitespace);
        if (initialSkip == std::string_view::npos) continue; // Skip empty lines
        line.remove_prefix(initialSkip);
        if (line.front() == ';') continue; // Skip comment lines
        // A value too large will yield a view to the end of the string, which is fine
        // It will cause the `=` to fail to be found
        auto propNameLen = line.find_first_of(ConfigManager::separators);
        std::string_view propName = line.substr(0, propNameLen);
        line.remove_prefix(propNameLen); // Skip property name

        auto middleSkip = line.find_first_not_of(ConfigManager::separators);
        if (middleSkip == std::string_view::npos) throw std::runtime_error(_path + " line " + std::to_string(lineNo) + " only contains a property name, but no value");
        line.remove_prefix(middleSkip);
        // Make sure to trim the value
        std::string_view property = line.substr(0, line.find_last_not_of(ConfigManager::whitespace) + 1);

        _properties.at(std::string(propName.begin(), propName.end()))->set(std::string(property.begin(), property.end()));
    }
}


#endif

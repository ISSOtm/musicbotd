#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP


#include <exception>
#include <fstream>
#include <locale>
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
    static std::map<int, void (ConfigManager::*)(std::ifstream &, std::locale const &)> _parsers;

private:
    std::string _path;

    std::map<std::string, std::unique_ptr<Property>> _properties;

    void parse_v1(std::ifstream & configFile, std::locale const & locale);

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

    // Try opening all INI files, grabbing the first matching one (the most specific)
    std::ifstream configFile;

    for (std::string const & path : { paths... }) {
        configFile = std::ifstream(path);

        if (!configFile.fail()) {
            _path = path;
            goto found; // C++ does not have `for {} else {}`, sadly...
        }

        spdlog::get("logger")->warn("Failed to open ini file {}, falling back...", path);
    }
    throw std::runtime_error("Failed to open any ini file");

found:
    // We want a unique locale for parsing option files, nothing system-dependent
    std::locale const & locale = std::locale::classic();
    configFile.imbue(locale);

    // Read the config from the file

    // First, expect a version line
    unsigned version;
    if ([&configFile, &version]() {
        if (configFile.get() != '#') return true;
        std::string s;
        configFile >> s;
        if (s != "version") return true;
        configFile >> version;
        return configFile.fail();
    }()) {
        throw std::runtime_error(_path + ": Expected a version line on line 1");
    }

    // Now, parse config lines
    try {
        (this->*_parsers.at(version))(configFile, locale);
    } catch (std::out_of_range const &) {
        throw std::out_of_range(_path + ": Unsupported version " + std::to_string(version));
    }
}


#endif

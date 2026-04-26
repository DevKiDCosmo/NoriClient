#pragma once

#include <fstream>
#include <optional>
#include <sstream>
#include <string>

class env {
public:
    struct EnvConfig {
        std::string server;
        std::string port;
        bool biometricRequired = true;
        bool debugMode = false;
        std::string dialogIconPath;
        std::string appIconPath;
    };

    static std::optional<EnvConfig> loadEnvConfig(const std::string &envPath);

    // private:

    static std::string trim(const std::string &value);
    static std::string toLower(std::string text);
    static bool parseBool(const std::string &value, bool defaultValue);
};

#include "env.h"
#include "../../logs/logger.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>



std::string env::trim(const std::string &value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });

    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();

    if (first >= last) {
        return {};
    }

    return {first, last};
}

std::string env::toLower(std::string text) {
    std::ranges::transform(text, text.begin(),
        [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    return text;
}

bool env::parseBool(const std::string &value, bool defaultValue) {
    const std::string processedValue = toLower(trim(value));
    if (processedValue == "1" || processedValue == "true" || processedValue == "yes" || processedValue == "on") {
        return true;
    }
    if (processedValue == "0" || processedValue == "false" || processedValue == "no" || processedValue == "off") {
        return false;
    }
    return defaultValue;
}

std::optional<env::EnvConfig> env::loadEnvConfig(const std::string &envPath) {
    std::ifstream envFile(envPath);
    if (!envFile) {
        logger::error("Could not open env file: " + envPath);
        return std::nullopt;
    }

    EnvConfig config;
    std::string line;
    while (std::getline(envFile, line)) {
        const std::string normalized = trim(line);
        if (normalized.empty() || normalized.starts_with('#')) {
            continue;
        }

        const auto delimiterPos = normalized.find('=');
        if (delimiterPos == std::string::npos) {
            continue;
        }

        std::string key = toLower(trim(normalized.substr(0, delimiterPos)));
        const std::string value = trim(normalized.substr(delimiterPos + 1));

        if (key == "server") {
            config.server = value;
        } else if (key == "port") {
            config.port = value;
        } else if (key == "biometric_required") {
            config.biometricRequired = parseBool(value, true);
        } else if (key == "type") {
            config.debugMode = (toLower(value) == "debug");
        } else if (key == "dialog_icon_path") {
            config.dialogIconPath = value;
        } else if (key == "app_icon_path") {
            config.appIconPath = value;
        }
    }

    if (config.server.empty() || config.port.empty()) {
        logger::error("Missing 'server' or 'port' in " + envPath);
        return std::nullopt;
    }

    logger::init("Configuration loaded successfully.");
    logger::init("- Server: " + config.server);
    logger::init("- Port: " + config.port);
    logger::init("- Biometric Required: " + std::string(config.biometricRequired ? "true" : "false"));
    logger::init("- Debug Mode: " + std::string(config.debugMode ? "true" : "false"));
    if (!config.dialogIconPath.empty()) {
        logger::init("- Dialog Icon: " + config.dialogIconPath);
    }
    if (!config.appIconPath.empty()) {
        logger::init("- App Icon: " + config.appIconPath);
    }

    return config;
}

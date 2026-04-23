#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "biometric/BiometricAuth.h"
#include "logs/logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

struct EnvConfig {
    std::string server;
    std::string port;
    bool biometricRequired = true;
};

struct HttpResponse {
    long statusCode = 0;
    std::string body;
};

std::string trim(const std::string &value) {
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

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool parseBool(std::string value, bool defaultValue) {
    value = toLower(trim(value));
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return defaultValue;
}

std::optional<EnvConfig> loadEnvConfig(const std::string &envPath) {
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
        }
    }

    if (config.server.empty() || config.port.empty()) {
        logger::error("Missing 'server' or 'port' in " + envPath);
        return std::nullopt;
    }

    return config;
}

size_t writeCallback(char *contents, size_t size, size_t nmemb, void *userp) {
    const size_t realSize = size * nmemb;
    auto *body = static_cast<std::string *>(userp);
    body->append(contents, realSize);
    return realSize;
}

std::optional<HttpResponse> fetchRoot(const std::string &url, bool biometricRequired) {
    if (biometricRequired) {
        std::string authError;
        if (!biometric::authorizeRequest("Authenticate to prove identity", authError)) {
            logger::warning("Biometric authentication failed: " + authError);
            return std::nullopt;
        }
        logger::system("Biometric authentication successful.");
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        logger::fatal("Failed to initialize curl");
        return std::nullopt;
    }

    HttpResponse response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NoriID/1.0");

    // Custom header
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "X-NoriID-Client: true");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-Request-Source: desktop");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        logger::error(std::string("Request failed: ") + curl_easy_strerror(result));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return std::nullopt;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    logger::information("NoriID client started.");

    const std::string envPath = "data/.env";
    const auto config = loadEnvConfig(envPath);
    if (!config) {
        curl_global_cleanup();
        return 1;
    }

    std::ostringstream url;
    url << "https://" << config->server << ':' << config->port << '/';

    logger::api("Connecting to " + url.str());

    const auto response = fetchRoot(url.str(), config->biometricRequired);
    if (!response) {
        curl_global_cleanup();
        logger::error("Failed to fetch root request.");
        return 1;
    }

    if (response->statusCode < 200 || response->statusCode >= 300) {
        logger::warning("Server returned HTTP " + std::to_string(response->statusCode));
        curl_global_cleanup();
        logger::error(std::string("Failed to fetch root request: ") + response->body);
        return 1;
    }

    try {
        const nlohmann::json jsonResponse = nlohmann::json::parse(response->body);
        logger::response("HTTP " + std::to_string(response->statusCode) + " JSON response:");
        logger::information(jsonResponse.dump(2));
    } catch (const nlohmann::json::parse_error &err) {
        logger::error(std::string("Failed to parse JSON: ") + err.what());
        curl_global_cleanup();
        return 1;
    }

    logger::important("Request completed successfully.");
    curl_global_cleanup();
    return 0;
}
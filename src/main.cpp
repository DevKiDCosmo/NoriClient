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
    bool debugMode = false;
    std::string dialogIconPath;
};

struct HttpResponse {
    long statusCode = 0;
    std::string body;
};

enum class FetchRootStatus {
    Success,
    AuthCanceled,
    Failed
};

struct FetchRootResult {
    FetchRootStatus status = FetchRootStatus::Failed;
    std::optional<HttpResponse> response;
    std::string message;
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
        } else if (key == "type") {
            config.debugMode = (toLower(value) == "debug");
        } else if (key == "dialog_icon_path") {
            config.dialogIconPath = value;
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

FetchRootResult fetchRoot(const std::string &url, bool biometricRequired, bool debugMode, const std::string &dialogIconPath) {
    if (biometricRequired) {
        const biometric::AuthResult authResult = biometric::authorizeRequest("Authenticate to prove identity", debugMode, dialogIconPath);
        if (authResult == biometric::AuthResult::Canceled) {
            return {FetchRootStatus::AuthCanceled, std::nullopt, "Authentication canceled by user."};
        }
        if (authResult != biometric::AuthResult::Success) {
            return {FetchRootStatus::Failed, std::nullopt, "Authentication failed."};
        }
        logger::system("Biometric authentication successful.");
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        logger::fatal("Failed to initialize curl");
        return {FetchRootStatus::Failed, std::nullopt, "Failed to initialize curl"};
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
        const std::string error = std::string("Request failed: ") + curl_easy_strerror(result);
        logger::error(error);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return {FetchRootStatus::Failed, std::nullopt, error};
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return {FetchRootStatus::Success, response, {}};
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    biometric::initializeUiHost();
    logger::information("NoriID client started.");

    const std::string envPath = "data/.env";
    const auto config = loadEnvConfig(envPath);
    if (!config) {
        curl_global_cleanup();
        return 1;
    }

    std::ostringstream url;
    url << "https://" << config->server << ':' << config->port << "/api/v0.1";

    logger::api("Connecting to " + url.str());

    const FetchRootResult fetchResult = fetchRoot(url.str(), config->biometricRequired, config->debugMode, config->dialogIconPath);
    if (fetchResult.status == FetchRootStatus::AuthCanceled) {
        logger::fatal("Authentication canceled. Ending process normally.");
        curl_global_cleanup();
        return 0;
    }

    if (fetchResult.status != FetchRootStatus::Success || !fetchResult.response) {
        curl_global_cleanup();
        logger::error("Failed to fetch root request.");
        return 1;
    }

    const HttpResponse &response = *fetchResult.response;

    if (response.statusCode < 200 || response.statusCode >= 300) {
        logger::warning("Server returned HTTP " + std::to_string(response.statusCode));
        curl_global_cleanup();
        logger::error(std::string("Failed to fetch root request: ") + response.body);
        return 1;
    }

    try {
        const nlohmann::json jsonResponse = nlohmann::json::parse(response.body);
        const std::string prettyJson = jsonResponse.dump(2);
        logger::response("HTTP " + std::to_string(response.statusCode) + " JSON response:");
        logger::information(prettyJson);
        biometric::showJsonResponseWindow(prettyJson, config->dialogIconPath);
    } catch (const nlohmann::json::parse_error &err) {
        logger::error(std::string("Failed to parse JSON: ") + err.what());
        curl_global_cleanup();
        return 1;
    }

    logger::important("Request completed successfully.");
    curl_global_cleanup();
    return 0;
}
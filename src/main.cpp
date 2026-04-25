#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "biometric/BiometricAuth.h"
#include "ui/AppController.h"
#include "logs/logger.h"
#include "defines/defines.h"

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
    std::string appIconPath;
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
        } else if (key == "app_icon_path") {
            config.appIconPath = value;
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

struct ParsedUri {
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;
    std::string query;
};

ParsedUri parseUri(const std::string &uri) {
    ParsedUri parsed;
    const auto schemePos = uri.find("://");
    if (schemePos == std::string::npos) {
        return parsed;
    }

    parsed.scheme = toLower(uri.substr(0, schemePos));
    const std::string remainder = uri.substr(schemePos + 3);
    const auto pathPos = remainder.find('/');
    const std::string authority = pathPos == std::string::npos ? remainder : remainder.substr(0, pathPos);
    const std::string pathAndQuery = pathPos == std::string::npos ? std::string{} : remainder.substr(pathPos);

    const auto colonPos = authority.rfind(':');
    if (colonPos != std::string::npos && colonPos + 1 < authority.size()) {
        parsed.host = authority.substr(0, colonPos);
        parsed.port = authority.substr(colonPos + 1);
    } else {
        parsed.host = authority;
    }

    const auto queryPos = pathAndQuery.find('?');
    parsed.path = queryPos == std::string::npos ? pathAndQuery : pathAndQuery.substr(0, queryPos);
    parsed.query = queryPos == std::string::npos ? std::string{} : pathAndQuery.substr(queryPos + 1);
    return parsed;
}

std::string buildOpenPortsSummary(const EnvConfig &config) {
    std::ostringstream summary;
    summary << "Version: " << VERSION << '\n'
            << "Version name: " << VERSION_NAME << '\n'
            << COPYRIGHT << '\n'
            << "API endpoint: https://" << config.server << ':' << config.port << "/api/v0.1\n"
            << "URI scheme: nori-slk://host[:port]/auth\n"
            << "Callback scheme: nori-api://";
    return summary.str();
}

FetchRootResult fetchRoot(const std::string &url, bool biometricRequired, bool debugMode, const std::string &dialogIconPath);

void processUri(const std::string &uri, const EnvConfig &config) {
    const ParsedUri parsed = parseUri(uri);
    if (parsed.scheme.empty()) {
        logger::warning("Unsupported URI: " + uri);
        return;
    }

    if (parsed.scheme == "nori-slk") {
        logger::socket("Received nori-slk URI: " + uri);
        if (!parsed.host.empty()) {
            logger::socket("nori-slk source host: " + parsed.host + (parsed.port.empty() ? std::string{} : (std::string(":") + parsed.port)));
        }
        if (!parsed.path.empty()) {
            logger::socket("nori-slk source path: " + parsed.path);
        }

        std::ostringstream target;
        //target << "https://" << config.server << ':' << config.port;
        target << "http://" << parsed.host << ":" << parsed.port << parsed.path;
        logger::api(target.str());

        const FetchRootResult fetchResult = fetchRoot(target.str(), config.biometricRequired, config.debugMode, config.dialogIconPath);
        if (fetchResult.status == FetchRootStatus::AuthCanceled) {
            logger::fatal("Authentication canceled. Request aborted.");
            return;
        }

        if (fetchResult.status != FetchRootStatus::Success || !fetchResult.response) {
            logger::error("Failed to fetch nori-slk request.");
            biometric::showJsonResponseWindow("Failed to fetch nori-slk request.", config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
            return;
        }

        const HttpResponse &response = *fetchResult.response;
        if (response.statusCode < 200 || response.statusCode >= 300) {
            logger::warning("nori-slk request returned HTTP " + std::to_string(response.statusCode));
            biometric::showJsonResponseWindow(response.body, config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
            return;
        }

        try {
            const nlohmann::json jsonResponse = nlohmann::json::parse(response.body);
            const std::string prettyJson = jsonResponse.dump(2);
            logger::response("nori-slk JSON response received.");
            biometric::showJsonResponseWindow(prettyJson, config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
        } catch (const nlohmann::json::parse_error &err) {
            logger::error(std::string("Failed to parse nori-slk JSON: ") + err.what());
            biometric::showJsonResponseWindow(response.body, config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
        }
        return;
    }

    if (parsed.scheme == "nori-api") {
        logger::api("Received nori-api URI: " + uri);
        ui::showInfoWindow();
        return;
    }

    logger::warning("Unknown URI scheme: " + parsed.scheme);
}

FetchRootResult fetchRoot(const std::string &url, bool biometricRequired, bool debugMode, const std::string &dialogIconPath) {
    if (biometricRequired) {
        const biometric::AuthResult authResult = biometric::authorizeRequest("Authenticate to prove identity", debugMode, dialogIconPath);
        if (authResult == biometric::AuthResult::Canceled || authResult == biometric::AuthResult::Failed) {
            return {FetchRootStatus::AuthCanceled, std::nullopt, "Authentication canceled by user."};
        }
        if (authResult != biometric::AuthResult::Success) {
            return {FetchRootStatus::Failed, std::nullopt, "Authentication failed."};
        }
        logger::system("Biometric authentication successful.");
    } else {
        logger::hint("Authentication disabled.");
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

int main(int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    logger::information("NoriID client started.");
    // logger::information("VERSION: " + VERSION);

    const std::string envPath = "data/.env";
    const auto config = loadEnvConfig(envPath);
    if (!config) {
        curl_global_cleanup();
        return 1;
    }

    biometric::initializeUiHost();

    const std::string version = std::string(VERSION) + " " + std::string(VERSION_NAME);
    const std::string openPorts = buildOpenPortsSummary(*config);

    ui::installAppController(version, openPorts, config->appIconPath, [config](const std::string &uri) {
        processUri(uri, *config);
    });

    logger::important("Background service ready. Click the lock icon for details.");

    if (argc > 1) {
        for (int index = 1; index < argc; ++index) {
            processUri(argv[index], *config);
        }
    }

    ui::runApplication();
    curl_global_cleanup();
    return 0;
}
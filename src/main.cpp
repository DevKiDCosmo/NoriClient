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
#include <vector>

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

    // The authority is the part before the first slash (e.g., host:port)
    const auto pathPos = remainder.find('/');
    const std::string authority = (pathPos == std::string::npos) ? remainder : remainder.substr(0, pathPos);
    const std::string pathAndQuery = (pathPos == std::string::npos) ? "" : remainder.substr(pathPos);

    // The host and port are within the authority
    const auto colonPos = authority.rfind(':');
    if (colonPos != std::string::npos) {
        parsed.host = authority.substr(0, colonPos);
        parsed.port = authority.substr(colonPos + 1);
    } else {
        parsed.host = authority;
    }

    // The path and query are the rest of the string
    const auto queryPos = pathAndQuery.find('?');
    parsed.path = queryPos == std::string::npos ? pathAndQuery : pathAndQuery.substr(0, queryPos);
    parsed.query = queryPos == std::string::npos ? std::string{} : pathAndQuery.substr(queryPos + 1);
    return parsed;
}

std::string urlDecode(const std::string &encoded) {
    std::string decoded;
    decoded.reserve(encoded.length());
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            const std::string hex = encoded.substr(i + 1, 2);
            try {
                const char c = static_cast<char>(std::stoi(hex, nullptr, 16));
                decoded += c;
                i += 2;
            } catch (const std::invalid_argument &) {
                // Not a valid hex, append as-is
                decoded += encoded[i];
            } catch (const std::out_of_range &) {
                // Not a valid hex, append as-is
                decoded += encoded[i];
            }
        } else if (encoded[i] == '+') {
            // In query strings, '+' is often a space.
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

/**
 * @brief Decodes only a "safe" set of percent-encoded characters.
 *
 * This function is designed for logging and inspection. It decodes common
 * characters like spaces (%20), quotes (%22), and brackets (%7B, %7D),
 * but leaves other sequences (like raw binary data or complex UTF-8) in their
 * percent-encoded form (%XX). This makes the resulting string safe to print
 * and easier to read in logs.
 *
 * @param encoded The URL-encoded string.
 * @return A partially decoded, log-safe string.
 */
std::string urlDecodeSafe(const std::string &encoded) {
    std::string decoded;
    decoded.reserve(encoded.length());
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            const std::string hex = toLower(encoded.substr(i + 1, 2));
            // Only decode a specific "safe" list of characters.
            // Everything else remains encoded for inspection.
            if (hex == "20" || hex == "22" || hex == "2f" || hex == "3a" || hex == "3c" || hex == "3e" || hex == "5b" || hex == "5d" || hex == "7b" || hex == "7d") { // space, ", /, :, <, >, [, ], {, }
                decoded += static_cast<char>(std::stoi(hex, nullptr, 16));
                i += 2;
            } else {
                decoded += encoded.substr(i, 3); // Append the %xx sequence as-is
                i += 2;
            }
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

std::string buildOpenPortsSummary(const EnvConfig &config) {
    std::ostringstream summary;
    summary << "Version: " << VERSION << '\n'
            << "Version name: " << VERSION_NAME << '\n'
            << COPYRIGHT << '\n'
            << "API endpoint: https://" << config.server << ':' << config.port << "/api/v0.1\n"
            << "URI scheme: nori-slk://host[:port]/auth\n"
            << "Callback scheme: nori-api://\n"
            << "Auth scheme: nori-auth://\n"
            << "Request scheme: nori-request://\n";
    return summary.str();
}

std::optional<uint32_t> findMagicNumberPurpose(const std::string &data) {
    // New magic number: 0xC4B1B68A81D54265
    const std::vector<unsigned char> magicBytes = {0xC4, 0xB1, 0xB6, 0x8A, 0x81, 0xD5, 0x42, 0x65};
    const size_t magicSize = magicBytes.size();

    // The purpose is a 4-byte integer following the magic number.
    const size_t purposeSize = sizeof(uint32_t);

    if (data.length() < magicSize + purposeSize) {
        return std::nullopt; // Not enough data to contain magic + purpose
    }

    const auto it = std::search(data.begin(), data.end(), magicBytes.begin(), magicBytes.end());

    if (it != data.end() && std::distance(it, data.end()) >= static_cast<long>(magicSize + purposeSize)) {
        uint32_t purpose = 0;
        std::memcpy(&purpose, &*(it + magicSize), purposeSize);
        return purpose;
    }
    return std::nullopt;
}

std::optional<uint32_t> findMagicNumberInUrlEncoded(const std::string &encodedData) {
    // New magic number: 0xC4B1B68A81D54265
    // This is the URL-encoded representation of the magic number bytes.
    const std::string magicString = "%C4%B1%B6%8A%81%D5%42%65";
    const auto it = std::search(
        encodedData.begin(), encodedData.end(),
        magicString.begin(), magicString.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );

    if (it == encodedData.end()) {
        return std::nullopt;
    }

    // The purpose is a 4-byte integer following the magic number.
    // In a URL, this would likely be encoded as 8 hex characters.
    const size_t purposeHexLength = sizeof(uint32_t) * 2;
    const auto purposeStart = it + magicString.length();

    // Skip any intermediate whitespace between magic number and purpose code.
    auto purposeIt = purposeStart;
    while (purposeIt != encodedData.end() && std::isspace(static_cast<unsigned char>(*purposeIt))) {
        ++purposeIt;
    }

    if (std::distance(purposeIt, encodedData.end()) < static_cast<long>(purposeHexLength)) {
        return std::nullopt;
    }

    const std::string purposeHex = std::string(purposeIt, purposeIt + purposeHexLength);
    try {
        return static_cast<uint32_t>(std::stoul(purposeHex, nullptr, 16));
    } catch (const std::invalid_argument &) {
        logger::warning("Could not parse purpose code from hex: '" + purposeHex + "'");
        return std::nullopt;
    } catch (const std::out_of_range &) {
        logger::warning("Purpose code hex is out of range: '" + purposeHex + "'");
        return std::nullopt;
    }
}

std::optional<uint32_t> findMagicNumberInHexString(const std::string &hexData) {
    // New magic number: 0xC4B1B68A81D54265
    // This is the plain hexadecimal string representation.
    const std::string magicHexString = "C4B1B68A81D54265";
    const auto it = std::search(
        hexData.begin(), hexData.end(),
        magicHexString.begin(), magicHexString.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );

    if (it == hexData.end()) {
        return std::nullopt;
    }

    // The purpose is a 4-byte integer following the magic number,
    // represented as 8 hex characters.
    const size_t purposeHexLength = sizeof(uint32_t) * 2;
    const auto purposeStart = it + magicHexString.length();
    if (std::distance(purposeStart, hexData.end()) < static_cast<long>(purposeHexLength)) {
        return std::nullopt;
    }

    const std::string purposeHex = std::string(purposeStart, purposeStart + purposeHexLength);
    try {
        return static_cast<uint32_t>(std::stoul(purposeHex, nullptr, 16));
    } catch (const std::logic_error&) {
        return std::nullopt; // Not a valid hex string or out of range
    }
}

/**
 * @brief A debugging helper to find and log all percent-encoded sequences.
 *
 * When a magic number search fails, this function can be called to scan a string
 * and log all contiguous sequences of percent-encoded bytes. This helps verify
 * what potential "magic numbers" are actually present in the data.
 *
 * @param data The string to scan (e.g., a raw URI path).
 */
void logUrlEncodedCandidates(const std::string &data) {
    bool foundAny = false;
    for (size_t i = 0; i < data.length(); ++i) {
        if (i + 2 < data.length() && data[i] == '%') {
            // Found the start of a potential candidate block.
            std::string candidate;
            size_t j = i;
            while (j + 2 < data.length() && data[j] == '%') {
                candidate += data.substr(j, 3);
                j += 3;
            }

            // A magic number candidate should be at least a few bytes long.
            // This avoids logging single characters like %20 (space).
            if (candidate.length() >= 3 * 4) { // At least 4 bytes encoded
                logger::hint("Magic number verification: Found potential candidate in URI: " + candidate);
                foundAny = true;
            }
            i = j - 1; // Continue scanning after this block.
        }
    }
}

/**
 * @brief Finds the magic number in a URL-encoded string and extracts the following word as a string purpose.
 *
 * This function is designed to parse a protocol where a magic number is followed by a human-readable
 * command or "purpose" (e.g., "approval", "deny").
 *
 * @param encodedData The raw, URL-encoded string to search within.
 * @return An optional containing the purpose string if found, otherwise std::nullopt.
 */
std::optional<std::string> findMagicNumberAndPurposeString(const std::string &encodedData) {
    // New magic number: 0xC4B1B68A81D54265
    const std::string magicString = "%C4%B1%B6%8A%81%D5%42%65";
    const auto it = std::search(
        encodedData.begin(), encodedData.end(),
        magicString.begin(), magicString.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );

    if (it == encodedData.end()) {
        return std::nullopt;
    }

    // Skip the magic string and any following whitespace.
    auto purposeStart = it + magicString.length();
    while (purposeStart != encodedData.end() && std::isspace(static_cast<unsigned char>(*purposeStart))) {
        ++purposeStart;
    }

    // Find the end of the purpose word (the next whitespace or end of string).
    auto purposeEnd = purposeStart;
    while (purposeEnd != encodedData.end() && !std::isspace(static_cast<unsigned char>(*purposeEnd))) {
        ++purposeEnd;
    }

    return std::string(purposeStart, purposeEnd);
}

/**
 * @brief Removes the magic number and purpose from a raw URI path.
 *
 * This function finds the magic number in a path string and returns the "clean"
 * part of the path that comes before it. It's useful for extracting the core
 * resource path before making a request.
 *
 * @param rawPath The raw, percent-encoded path from a URI.
 * @return The path with the magic number and subsequent parts removed.
 */
std::string cleanPath(const std::string &rawPath) {
    const std::string magicString = "%C4%B1%B6%8A%81%D5%42%65";
    const auto it = std::search(
        rawPath.begin(), rawPath.end(),
        magicString.begin(), magicString.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );

    const auto endOfPath = std::find(rawPath.begin(), it, ' ');
    return std::string(rawPath.begin(), endOfPath);
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
        if (parsed.path.empty()) {
            return;
        }

        const std::string safeDecodedPath = urlDecodeSafe(parsed.path);
        logger::socket("nori-api path (safe decoded): " + safeDecodedPath);

        if (safeDecodedPath == "/info") {
            ui::showInfoWindow();
        } else {
            logger::api("Unknown nori-api path: " + safeDecodedPath);

            // Find the magic number and the string-based purpose that follows.
            if (const auto purpose = findMagicNumberAndPurposeString(parsed.path)) {
                logger::api("Magic number found in nori-api path with purpose: '" + *purpose + "'");
                if (*purpose == "approval") {
                    if (config.biometricRequired) {
                        const biometric::AuthResult authResult = biometric::authorizeRequest("Authenticate to reveal installation.", config.debugMode, config.dialogIconPath);
                        if (authResult != biometric::AuthResult::Success) {
                            logger::warning("Authentication was not successful. Approval action aborted.");
                            return; // Stop processing
                        }
                        logger::system("Biometric authentication successful.");
                    } else {
                        logger::hint("Authentication disabled.");
                    }

                    // The approval action is to make a web request, similar to nori-slk.
                    const std::string cleanedPath = cleanPath(parsed.path);
                    std::ostringstream target;
                    target << "http://" << parsed.host << ":" << parsed.port << cleanedPath;
                    logger::api("Dispatching nori-api request to: " + target.str());

                    const FetchRootResult fetchResult = fetchRoot(target.str(), false, config.debugMode, config.dialogIconPath);
                    if (fetchResult.status != FetchRootStatus::Success || !fetchResult.response) {
                        logger::error("Failed to fetch nori-api request.");
                        biometric::showJsonResponseWindow("Failed to fetch nori-api request.", config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
                        return;
                    }

                    const HttpResponse &response = *fetchResult.response;
                    if (response.statusCode < 200 || response.statusCode >= 300) {
                        logger::warning("nori-api request returned HTTP " + std::to_string(response.statusCode));
                        biometric::showJsonResponseWindow(response.body, config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
                        return;
                    }

                    try {
                        const nlohmann::json jsonResponse = nlohmann::json::parse(response.body);
                        logger::response("nori-api JSON response received.");
                        biometric::showJsonResponseWindow(jsonResponse.dump(2), config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
                    } catch (const nlohmann::json::parse_error &err) {
                        logger::error(std::string("Failed to parse nori-api JSON: ") + err.what());
                        biometric::showJsonResponseWindow(response.body, config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
                    }
                } else if (*purpose == "registration") {
                    // This will do client registration.
                }
            } else {
                logger::warning("Magic number not found in the URI path.");
                logUrlEncodedCandidates(parsed.path); // Log candidates for debugging.
            }
        }
        return;
    }

    if (parsed.scheme == "nori-auth") {
        logger::warning("Received nori-auth URI: " + uri);
        if (parsed.path.empty()) {
            return;
        }

        const std::string safeDecodedPath = urlDecodeSafe(parsed.path);
        logger::socket("nori-auth path (safe decoded): " + safeDecodedPath);

        if (safeDecodedPath == "/info") {
            ui::showInfoWindow();
        } else {
            logger::api("Unknown nori-auth path: " + safeDecodedPath);
            // TODO: Add logic to handle other nori-auth paths.
            // For example, you could parse the decodedPath as a JSON object.
        }
        return;
    }

    if (parsed.scheme == "nori-request") {
        logger::warning("Received nori-request URI: " + uri);
        // This is a placeholder for how you might use the magic number function.
        // You would likely get the data from the URI path or query.
        const std::string requestData = "some_data...<magic_number><purpose_code>...more_data";
        const auto purpose = findMagicNumberPurpose(requestData);
        // processRequest(purpose);
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
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); // NOLINT(cppcoreguidelines-pro-type-vararg)
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

    // ! TODO: AppIconPath or the image cannot be applied.
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
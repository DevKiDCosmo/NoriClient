#include "utilURI.h"

#include <vector>
#include <algorithm>
#include <fstream>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <thread>

#include "../../../defines/defines.h"
#include "../../../defines/env/env.h"
#include "../../../logs/logger.h"

uri::MICS::ParsedUri uri::MICS::parseUri(const std::string &uri) {
    uri::MICS::ParsedUri parsed;
    const auto schemePos = uri.find("://");
    if (schemePos == std::string::npos) {
        return parsed;
    }

    parsed.scheme = env::toLower(uri.substr(0, schemePos));
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

std::string uri::MICS::urlDecode(const std::string &encoded) {
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
                decoded += encoded[i];
            } catch (const std::out_of_range &) {
                decoded += encoded[i];
            }
        } else if (encoded[i] == '+') {
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
std::string uri::MICS::urlDecodeSafe(const std::string &encoded) {
    std::string decoded;
    decoded.reserve(encoded.length());
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            const std::string hex = env::toLower(encoded.substr(i + 1, 2));
            // Only decode a specific "safe" list of characters.
            // Everything else remains encoded for inspection.
            if (hex == "20" || hex == "22" || hex == "2f" || hex == "3a" || hex == "3c" || hex == "3e" || hex == "5b" ||
                hex == "5d" || hex == "7b" || hex == "7d") {
                // space, ", /, :, <, >, [, ], {, }
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

std::string uri::MICS::buildOpenPortsSummary(const env::EnvConfig &config) {
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

/**
 * @brief A debugging helper to find and log all percent-encoded sequences.
 *
 * When a magic number search fails, this function can be called to scan a string
 * and log all contiguous sequences of percent-encoded bytes. This helps verify
 * what potential "magic numbers" are actually present in the data.
 *
 * @param data The string to scan (e.g., a raw URI path).
 */
void uri::MICS::logUrlEncodedCandidates(const std::string &data) {
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
            if (candidate.length() >= 3 * 4) {
                // At least 4 bytes encoded
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
std::optional<std::string> uri::MICS::findMagicNumberAndPurposeString(const std::string &encodedData) {
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
std::string uri::MICS::cleanPath(const std::string &rawPath) {
    const std::string magicString = "%C4%B1%B6%8A%81%D5%42%65";
    const auto it = std::search(
        rawPath.begin(), rawPath.end(),
        magicString.begin(), magicString.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );

    const auto endOfPath = std::find(rawPath.begin(), it, ' ');
    return std::string(rawPath.begin(), endOfPath);
}

/**
 * @brief Removes the magic number and purpose from a raw URI path.
 *
 * This function searches for a predefined "magic number" marker inside a
 * percent-encoded URI path and returns only the portion of the path that
 * appears before that marker. If the marker is not found, the original
 * path is returned unchanged.
 *
 * Example:
 *   rawPath: "/api/v1/resource/12345 {MAGIC+PURPOSE}"
 *   result:  {"api", "v1", "resource", "12345"}
 *
 * @param rawPath The raw, percent-encoded path from a URI.
 * @return A cleaned path with the magic number and any following segments removed.
 */
std::vector<std::string> uri::MICS::splitPath(std::string_view rawPath) {
    const std::string cleaned = cleanPath(std::string(rawPath));

    std::vector<std::string> result;

    for (auto &&part: cleaned | std::views::split('/')) {
        std::string segment(part.begin(), part.end());
        if (!segment.empty()) {
            result.push_back(std::move(segment));
        }
    }

    return result;
}

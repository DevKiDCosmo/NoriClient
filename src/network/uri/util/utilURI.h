#pragma once

#include "../../../defines/env/env.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uri {
    namespace validate {
        class Host {
        public:
            static bool validateHost(std::string host);

            static bool validateIP(std::string host);
        };
    }

    class MICS {
    public:
        struct ParsedUri {
            std::string scheme;
            std::string host;
            std::string port;
            std::string path;
            std::string query;
        };

        static ParsedUri parseUri(const std::string &uri);
        static std::string urlDecode(const std::string &encoded);
        static std::string urlDecodeSafe(const std::string &encoded);
        static void logUrlEncodedCandidates(const std::string &data);
        static std::optional<std::string> findMagicNumberAndPurposeString(const std::string &encodedData);
        static std::string cleanPath(const std::string &rawPath);
        static std::vector<std::string> splitPath(std::string_view rawPath);
        static std::string buildOpenPortsSummary(const env::EnvConfig &config);
    };
}

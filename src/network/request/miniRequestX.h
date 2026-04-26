#pragma once

#include <string>
#include <optional>
#include <vector>
#include <regex>

namespace network::request {
    class ProtocolChain {
    public:
        static ProtocolChain create(std::initializer_list<std::string> input) {
            ProtocolChain chain;

            for (const auto& proto : input) {
                if (!isValid(proto)) {
                    throw std::invalid_argument("Invalid protocol: " + proto);
                }
                chain.protocols_.push_back(proto);
            }

            return chain;
        }

        const std::vector<std::string>& list() const noexcept {
            return protocols_;
        }

    private:
        std::vector<std::string> protocols_;

        static bool isValid(const std::string& proto) {
            static const std::regex r(R"(^[a-zA-Z][a-zA-Z0-9+.-]*://$)");
            return std::regex_match(proto, r);
        }
    };

    struct HttpResponse {
        long statusCode = 0;
        std::string body;
    };

    enum class FetchStatus {
        Success,
        AuthCanceled,
        Failed
    };

    struct FetchResult {
        FetchStatus status = FetchStatus::Failed;
        std::optional<HttpResponse> response;
        std::string message;
    };

    class MiniRequest {
    public:
        static const ProtocolChain chainHTTP;

        static FetchResult fetch(const std::string &url, bool biometricRequired, bool debugMode,
                                 const std::string &dialogIconPath, const ProtocolChain &chain = chainHTTP);

        static bool responseHandler(FetchResult &fetchResult, const bool &debugMode, const std::string &dialogIconPath);

    private:
        static FetchResult fetchRaw(const std::string &url, const std::string &dialogIconPath, const ProtocolChain &chain);
    };
}

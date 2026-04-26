#pragma once

#include <string>
#include <optional>

namespace network::request {
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
        static FetchResult fetch(const std::string &url, bool biometricRequired, bool debugMode,
                                 const std::string &dialogIconPath);

        static bool responseHandler(FetchResult &fetchResult, const bool &debugMode, const std::string &dialogIconPath);

    private:
        static FetchResult fetchRaw(const std::string &url, const std::string &dialogIconPath);
    };
}

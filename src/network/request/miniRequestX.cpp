#include "miniRequestX.h"
#include "../../biometric/BiometricAuth.h"
#include "../../logs/logger.h"

#include <curl/curl.h>

namespace network::request {

    size_t writeCallback(char *contents, size_t size, size_t nmemb, void *userp) {
        const size_t realSize = size * nmemb;
        auto *body = static_cast<std::string *>(userp);
        body->append(contents, realSize);
        return realSize;
    }

    FetchResult MiniRequest::fetch(const std::string &url, bool biometricRequired, bool debugMode, const std::string &dialogIconPath) {
        if (biometricRequired) {
            const biometric::AuthResult authResult = biometric::authorizeRequest("Authenticate to prove identity", debugMode, dialogIconPath);
            if (authResult == biometric::AuthResult::Canceled || authResult == biometric::AuthResult::Failed) {
                return {FetchStatus::AuthCanceled, std::nullopt, "Authentication canceled by user."};
            }
            if (authResult != biometric::AuthResult::Success) {
                return {FetchStatus::Failed, std::nullopt, "Authentication failed."};
            }
            logger::system("Biometric authentication successful.");
        } else {
            logger::hint("Authentication disabled.");
        }
        return fetchRaw(url);
    }

    FetchResult MiniRequest::fetchRaw(const std::string &url) {
        CURL *curl = curl_easy_init();
        if (!curl) {
            logger::fatal("Failed to initialize curl");
            return {FetchStatus::Failed, std::nullopt, "Failed to initialize curl"};
        }

        HttpResponse response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "NoriID/1.0");

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "X-NoriID-Client: true");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "X-Request-Source: desktop-macos");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        const CURLcode result = curl_easy_perform(curl);
        if (result != CURLE_OK) {
            const std::string error = std::string("Request failed: ") + curl_easy_strerror(result);
            logger::error(error);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return {FetchStatus::Failed, std::nullopt, error};
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return {FetchStatus::Success, response, {}};
    }

}

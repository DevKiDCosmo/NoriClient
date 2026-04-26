#include "miniRequestX.h"
#include "../../biometric/BiometricAuth.h"
#include "../../logs/logger.h"
#include "../../ui/Dialogs.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace network::request {
    size_t writeCallback(char *contents, size_t size, size_t nmemb, void *userp) {
        const size_t realSize = size * nmemb;
        auto *body = static_cast<std::string *>(userp);
        body->append(contents, realSize);
        return realSize;
    }

    int progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
        if (dltotal > 0) {
            double fraction = static_cast<double>(dlnow) / static_cast<double>(dltotal);
            ui::updateProgress(fraction);
        }
        return 0;
    }

    FetchResult MiniRequest::fetch(const std::string &url, bool biometricRequired, bool debugMode,
                                   const std::string &dialogIconPath) {
        if (biometricRequired) {
            const biometric::AuthResult authResult = biometric::authorizeRequest(
                "Authenticate to prove identity", debugMode, dialogIconPath);
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
        return fetchRaw(url, dialogIconPath);
    }

    bool MiniRequest::responseHandler(FetchResult &fetchResult, const bool &debugMode,
                                      const std::string &dialogIconPath) {
        if (fetchResult.status != network::request::FetchStatus::Success || !fetchResult.response) {
            logger::error("Failed to fetch nori-api request.");
            ui::showButtonDialog("Request Failed", "Could not connect to the server.", dialogIconPath, "OK", "",
                                 nullptr);
            return false;
        }

        const network::request::HttpResponse &response = *fetchResult.response;
        if (response.statusCode < 200 || response.statusCode >= 300) {
            logger::warning("nori-api request returned HTTP " + std::to_string(response.statusCode));
            ui::showButtonDialog("Request Error", response.body, dialogIconPath, "OK", "", nullptr);
            return false;
        }

        try {
            const nlohmann::json jsonResponse = nlohmann::json::parse(response.body);
            logger::response("nori-api JSON response received.");
            logger::information(jsonResponse.dump(2));

            if (jsonResponse.contains("status")) {
                std::string status = jsonResponse["status"];
                std::string message = "No message provided."; // Default message

                if (jsonResponse.contains("message")) {
                    message = jsonResponse["message"];
                }

                if (status == "approved") {
                    ui::showButtonDialog("Success", "Program Validation succeed.", dialogIconPath, "OK", "", nullptr);
                } else if (status == "invalid") {
                    ui::showButtonDialog("Invalid Request", "The code was either used before is scrambled.",
                                         dialogIconPath, "OK", "", nullptr);
                } else if (status == "value-error") {
                    ui::showButtonDialog("Value Error", message, dialogIconPath, "OK", "", nullptr);
                } else {
                    ui::showButtonDialog("Unknown Response", response.body, dialogIconPath, "OK", "", nullptr);
                }
            } else {
                if (debugMode)
                    biometric::showJsonResponseWindow(jsonResponse.dump(2), dialogIconPath);
            }
        } catch (const nlohmann::json::parse_error &err) {
            logger::error(std::string("Failed to parse nori-api JSON: ") + err.what());
            logger::information("Raw response body: " + response.body);
            ui::showButtonDialog("JSON Error", "Failed to parse the server response.", dialogIconPath, "OK", "",
                                 nullptr);
        }
        return true;
    }


    FetchResult MiniRequest::fetchRaw(const std::string &url, const std::string &dialogIconPath) {
        CURL *curl = curl_easy_init();
        if (!curl) {
            logger::fatal("Failed to initialize curl");
            return {FetchStatus::Failed, std::nullopt, "Failed to initialize curl"};
        }

        ui::showProgressDialog("Connecting...", "Fetching data from the server.", dialogIconPath);

        HttpResponse response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "NoriID/1.0");
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "X-NoriID-Client: true");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "X-Request-Source: desktop-macos");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        const CURLcode result = curl_easy_perform(curl);

        ui::closeProgressDialog();

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

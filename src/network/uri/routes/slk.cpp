#include <string_view>

#include "routes.h"
#include "../uriHandler.h"
#include "../../../biometric/BiometricAuth.h"
#include "../../../logs/logger.h"
#include "../../request/miniRequestX.h"

void routes::slk::slk_(uri::MICS::ParsedUri parsed, const std::string &uri, const env::EnvConfig &config) {
    logger::socket("Received nori-slk URI: " + uri);
    if (!parsed.host.empty()) {
        logger::socket(
            "nori-slk source host: " + parsed.host + (parsed.port.empty()
                                                          ? std::string{}
                                                          : (std::string(":") + parsed.port)));
    }
    if (!parsed.path.empty()) {
        logger::socket("nori-slk source path: " + parsed.path);
    }

    std::ostringstream target;
    //target << "https://" << config.server << ':' << config.port;
    target << parsed.host << ":" << parsed.port << parsed.path;
    logger::api(target.str());

    const network::request::FetchResult fetchResult = network::request::MiniRequest::fetch(
        target.str(), config.biometricRequired, config.debugMode, config.dialogIconPath);
    if (fetchResult.status == network::request::FetchStatus::AuthCanceled) {
        logger::fatal("Authentication canceled. Request aborted.");
        return;
    }

    if (fetchResult.status != network::request::FetchStatus::Success || !fetchResult.response) {
        logger::error("Failed to fetch nori-slk request.");
        biometric::showJsonResponseWindow("Failed to fetch nori-slk request.",
                                          config.appIconPath.empty()
                                              ? config.dialogIconPath
                                              : config.appIconPath);
        return;
    }

    return;
}
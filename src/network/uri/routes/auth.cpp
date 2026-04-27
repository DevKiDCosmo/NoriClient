#include <string_view>

#include "routes.h"
#include "../uriHandler.h"
#include "../../../logs/logger.h"
#include "../../../ui/AppController.h"


void routes::auth::auth_(uri::MICS::ParsedUri parsed, const std::string &uri, const env::EnvConfig &config) {
    logger::warning("Received nori-auth URI: " + uri);
    if (parsed.path.empty()) {
        return;
    }

    const std::string safeDecodedPath = uri::MICS::urlDecodeSafe(parsed.path);
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

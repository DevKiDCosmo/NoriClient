#include <string_view>

#include "routes.h"
#include "../uriHandler.h"
#include "../../../logs/logger.h"
#include "../../../ui/AppController.h"


void routes::request::request_(uri::MICS::ParsedUri parsed, const std::string &uri, const env::EnvConfig &config) {
    logger::warning("Received nori-request URI: " + uri);
    if (const auto purpose = uri::MICS::findMagicNumberAndPurposeString(parsed.path)) {
        logger::notImplemented("nori-request with purpose '" + *purpose + "' is not implemented yet.");
    }
    return;
}

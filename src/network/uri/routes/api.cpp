#include "routes.h"
#include "../../../biometric/BiometricAuth.h"
#include "../../../logs/logger.h"
#include "../../../ui/AppController.h"
#include "../../request/miniRequestX.h"
#include "../util/utilURI.h"

void routes::api::api_(uri::MICS::ParsedUri parsed, const std::string &uri, const env::EnvConfig &config) {
    logger::api("Received nori-api URI: " + uri);

    logger::debug(
        "Validate host: " + std::string(uri::validate::Host::validateHost(parsed.host) ? "valid" : "invalid"));
    logger::debug("Validate IPvX: " + std::string(uri::validate::Host::validateIP(parsed.host) ? "valid" : "invalid"));

    /*
     * Adding first and second level parsing.
     * First:
     *   - Immediately after uri protocol. e.g. nori-api://{command}
     * Those usually have no continous segments but can.
     * Second.
     *   - All after the host: e.g. nori-api://{host}/...
     * Differenciating is happened in first position. Either a valid host or not, then it is automatically a command.
     */

    // Handle ambiguous "nori-api://info" case where "info" is parsed as host
    if (parsed.host == "info" && parsed.path.empty()) {
        ui::showInfoWindow();
        return;
    }

    if (parsed.path.empty()) {
        return;
    }

    const std::string safeDecodedPath = uri::MICS::urlDecodeSafe(parsed.path);
    logger::socket("nori-api path (safe decoded): " + safeDecodedPath);

    // ? HANDLE here first level.
    if (safeDecodedPath == "/info") {
        ui::showInfoWindow();
        return;
    }

    const auto segments = uri::MICS::splitPath(parsed.path);
    if (segments.empty()) {
        logger::warning("nori-api path is empty after splitting: " + parsed.path);
        return;
    }
    for (const std::string &seg: segments) {
        logger::debug("Segment: " + seg);
    }


    if (segments[0] == "info") {
        ui::showInfoWindow();
    } else {
        logger::api("Unknown nori-api path: " + safeDecodedPath);

        // Find the magic number and the string-based purpose that follows.
        if (const auto purpose = uri::MICS::findMagicNumberAndPurposeString(parsed.path)) {
            logger::api("Magic number found in nori-api path with purpose: '" + *purpose + "'");
            if (*purpose == "approval") {
                if (config.biometricRequired) {
                    const biometric::AuthResult authResult = biometric::authorizeRequest(
                        "Authenticate to reveal installation.", config.debugMode, config.dialogIconPath);
                    if (authResult != biometric::AuthResult::Success) {
                        logger::warning("Authentication was not successful. Approval action aborted.");
                        return;
                    }
                    logger::system("Biometric authentication successful.");
                } else {
                    logger::hint("Authentication disabled.");
                }

                const std::string cleanedPath = uri::MICS::cleanPath(parsed.path);
                std::ostringstream target;
                target << parsed.host << ":" << parsed.port << cleanedPath;
                logger::api("Dispatching nori-api request to: " + target.str());

                network::request::ProtocolChain chain = network::request::ProtocolChain::create({
                    "kaizo://", "kzps://", "nori-slk://", "nori-loop://", "nori-auth://", "https://", "http://"
                });

                auto fetchResult = network::request::MiniRequest::fetch(
                    target.str(), false, config.debugMode, config.dialogIconPath, chain);
                int reponseCode = network::request::MiniRequest::responseHandler(
                    fetchResult, config.debugMode, config.dialogIconPath);
                logger::debug(
                    "Code:" + std::to_string(reponseCode) + " - 1: Success / 0: Failed / -1: Canceled");

                // Fetch ID
                auto pathObj = uri::MICS::splitPath(cleanedPath);
                logger::information("ID: " + pathObj[pathObj.size() - 1]);
            } else if (*purpose == "registration") {
                // This will do client registration.
                logger::notImplemented("REGISTRATION purpose is not implemented yet in nori-api.");
            }
        } else {
            logger::warning("Magic number not found in the URI path.");
            uri::MICS::logUrlEncodedCandidates(parsed.path); // Log candidates for debugging.
        }
    }
    return;
}

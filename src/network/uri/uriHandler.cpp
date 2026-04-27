// ! Intern Lib Includes
#include "uriHandler.h"
#include "../../defines/env/env.h"
#include "../../logs/logger.h"

// ! Extern Lib Includes
#include <nlohmann/json.hpp>

// ! StdLib Includes
#include <string>
#include <thread>

#include "routes/routes.h"
#include "util/utilURI.h"

void routingHandler(const std::string &uri, const env::EnvConfig &config) {
    const uriHandler::ParsedUri parsed = uri::MICS::parseUri(uri);

    if (parsed.scheme.empty()) {
        logger::warning("Unsupported URI: " + uri);
        return;
    }

    if (parsed.scheme == "nori-slk")
        routes::slk::slk_(parsed, uri, config);


    if (parsed.scheme == "nori-api")
        routes::api::api_(parsed, uri, config);


    if (parsed.scheme == "nori-auth")
        routes::auth::auth_(parsed, uri, config);

    if (parsed.scheme == "nori-request")
        routes::request::request_(parsed, uri, config);

    logger::warning("Unknown URI scheme: " + parsed.scheme);
}

void uriHandler::processUri(const std::string &uri, const env::EnvConfig &config) {
    /* TODO: Every handlers are async.
     * TODO: If one endpoint is occupied then an job id is returned. with the job id a wss conenction can be established with
     * tells eventually when the job gets processes.
     * UI gets its own queue to handle error.
    */

    std::thread([uri, config] {
        routingHandler(uri, config);
    }).detach();

    //     std::jthread bgThreadURI = std::move(std::jthread([uri, config] { routingHandler(uri, config); }));
}

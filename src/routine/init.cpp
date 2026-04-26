#include "init.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "../biometric/BiometricAuth.h"
#include "../ui/AppController.h"
#include "../logs/logger.h"
#include "../defines/defines.h"
#include "../defines/env/env.h"
#include "../network/uri/uriHandler.h"

#include <algorithm>
#include <optional>
#include <string>

env::EnvConfig init::config;

int init::_init(int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    logger::information("NoriID client started.");
    // logger::information("VERSION: " + VERSION);

    const std::string envPath = "data/.env";
    auto loadedConfig = env::loadEnvConfig(envPath);
    if (!loadedConfig) {
        curl_global_cleanup();
        return 1;
    }
    config = *loadedConfig;

    biometric::initializeUiHost();

    const std::string version = std::string(VERSION) + " " + std::string(VERSION_NAME);
    const std::string openPorts = uriHandler::buildOpenPortsSummary(config);
    logger::init(openPorts);

    // ! TODO: AppIconPath or the image cannot be applied.
    // TODO: Bug where full OpenPortsSummary cannot be displayed. Assumption on Line Cap.
    ui::installAppController(version, openPorts, config.appIconPath, [](const std::string &uri) {
        uriHandler::processUri(uri, config);
    });

    logger::important("Background service ready. Click the lock icon for details.");

    if (argc > 1) {
        for (int index = 1; index < argc; ++index) {
            uriHandler::processUri(argv[index], config);
        }
    }

    ui::runApplication();
    curl_global_cleanup();
    return 0;
}

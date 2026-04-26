#pragma once

#include "../../defines/env/env.h"
#include <string>

class uriHandler {
public:
    static std::string buildOpenPortsSummary(const env::EnvConfig &config);

    static void processUri(const std::string &uri, const env::EnvConfig &config);

};

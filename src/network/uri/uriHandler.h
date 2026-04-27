#pragma once

#include "../../defines/env/env.h"
#include "util/utilURI.h"
#include <string>

class uriHandler {
public:
    using ParsedUri = uri::MICS::ParsedUri;

    static void processUri(const std::string &uri, const env::EnvConfig &config);
};

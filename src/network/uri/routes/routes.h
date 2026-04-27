#pragma once

#include "../../../defines/env/env.h"
#include "../util/utilURI.h"

#include <string>

namespace routes {
    class api {
    public:
        static void api_(uri::MICS::ParsedUri parsed, const std::string &uri, const env::EnvConfig &config);
    };

    class auth {
    public:
        static void auth_(uri::MICS::ParsedUri parsed, const std::string &uri,
                          const env::EnvConfig &config);
    };

    class request {
    public:
        static void request_(uri::MICS::ParsedUri parsed, const std::string &uri,
                                              const env::EnvConfig &config);
    };

    class slk {
    public:
        static void slk_(uri::MICS::ParsedUri parsed, const std::string &uri, const env::EnvConfig &config);
    };
}

#pragma once

#include <string>

namespace uri::validate {
    class Host {
    public:
        static bool validateHost(std::string host);
    };
}

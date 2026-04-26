#pragma once
#include "../defines/env/env.h"

class init {
public:
    static env::EnvConfig config;

    static int _init(int argc, char *argv[]);
};

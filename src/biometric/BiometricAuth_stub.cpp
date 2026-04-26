#include "BiometricAuth.h"

namespace biometric {
    void initializeUiHost() {
    }

    void showJsonResponseWindow(const std::string &, const std::string &) {
    }

    AuthResult authorizeRequest(const std::string &, bool, const std::string &) {
        return AuthResult::Failed;
    }
} // namespace biometric

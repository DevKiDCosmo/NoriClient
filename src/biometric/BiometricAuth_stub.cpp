#include "BiometricAuth.h"

namespace biometric {

bool authorizeRequest(const std::string &, std::string &errorMessage) {
    errorMessage = "Biometric authentication is only implemented for macOS in this build.";
    return false;
}

} // namespace biometric


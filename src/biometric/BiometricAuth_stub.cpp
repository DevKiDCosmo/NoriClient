#include "BiometricAuth.h"

namespace biometric {

void initializeUiHost() {}

void showJsonResponseWindow(const std::string &, const std::string &) {}

bool authorizeRequest(const std::string &, bool, const std::string &, std::string &errorMessage) {
    errorMessage = "Biometric authentication is only implemented for macOS in this build.";
    return false;
}

} // namespace biometric


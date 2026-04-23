#pragma once

#include <string>

namespace biometric {

// Triggers a local biometric prompt (Touch ID/Face ID) and returns true on success.
bool authorizeRequest(const std::string &reason, std::string &errorMessage);

} // namespace biometric


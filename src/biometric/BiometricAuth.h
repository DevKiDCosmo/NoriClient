#pragma once

#include <string>

namespace biometric {

// Initializes app host for authentication dialogs (no persistent dock icon).
void initializeUiHost();

// Shows a dedicated macOS window with JSON response text and optional app icon.
void showJsonResponseWindow(const std::string &jsonText, const std::string &appIconPath);

// Triggers a local biometric prompt (Touch ID/Face ID) and returns true on success.
bool authorizeRequest(const std::string &reason, bool debugMode, const std::string &dialogIconPath, std::string &errorMessage);

} // namespace biometric


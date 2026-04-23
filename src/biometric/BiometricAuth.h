#pragma once

#include <string>

namespace biometric {

enum class AuthResult {
    Success,
    Canceled,
    Failed
};

// Initializes app host for authentication dialogs (no persistent dock icon).
void initializeUiHost();

// Shows a dedicated macOS window with JSON response text and optional app icon.
void showJsonResponseWindow(const std::string &jsonText, const std::string &appIconPath);

// Triggers a local biometric/native auth prompt.
// Returns AuthResult::Success on success, AuthResult::Canceled if the user explicitly
// canceled, or AuthResult::Failed on any other failure.
AuthResult authorizeRequest(const std::string &reason, bool debugMode, const std::string &dialogIconPath);

} // namespace biometric


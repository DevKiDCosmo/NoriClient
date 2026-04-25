#pragma once

#include <string>

namespace biometric {

enum class AuthResult {
    Success,
    Canceled,
    Failed
};

// Initializes the macOS background host used for authentication dialogs and status-item UI.
void initializeUiHost();

// Shows a dedicated macOS window with JSON response text and optional app icon.
void showJsonResponseWindow(const std::string &jsonText, const std::string &appIconPath);

// Triggers a local biometric auth prompt first.
// If biometrics fail, the user is offered a native current-user password login path.
// Returns AuthResult::Success on success, AuthResult::Canceled if the user explicitly
// canceled, or AuthResult::Failed on any other failure.
AuthResult authorizeRequest(const std::string &reason, bool debugMode, const std::string &dialogIconPath);

} // namespace biometric


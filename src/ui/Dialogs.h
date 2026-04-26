#pragma once

#include <functional>
#include <string>

namespace ui {
    // Shows a dialog with a progress bar.
    void showProgressDialog(const std::string &message, const std::string &description, const std::string &iconPath);

    // Updates the progress bar in the dialog.
    void updateProgress(double fraction);

    // Closes the progress dialog.
    void closeProgressDialog();

    // Shows a dialog with a message and buttons.
    void showButtonDialog(const std::string &message, const std::string &description, const std::string &iconPath,
                          const std::string &button1Text, const std::string &button2Text,
                          std::function<void(int)> handler);
} // namespace ui

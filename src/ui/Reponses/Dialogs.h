#pragma once

#include <functional>
#include <string>
#include <vector>

namespace ui {
    enum class ProgressDialogFlag {
        Continuous,
        StatusChain
    };

    // Shows a dialog with a progress bar.
    void showProgressDialog(const std::string &message, const std::string &description, const std::string &iconPath,
                            ProgressDialogFlag flag = ProgressDialogFlag::Continuous);

    // Defines the ordered status stages for status-chain progress dialogs.
    void setProgressDialogStages(const std::vector<std::string> &stages);

    // Advances to the next status stage in status-chain mode.
    void activateProgressDialogStage();

    // Registers a function that runs continuously while the progress dialog is open.
    void functionProgressDialog(const std::function<void()> &handler);

    // Updates the progress bar in the dialog.
    void updateProgress(double fraction);

    // Closes the progress dialog.
    void closeProgressDialog();


    // Shows a dialog with a message and buttons.
    void showButtonDialog(const std::string &message, const std::string &description, const std::string &iconPath,
                          const std::string &button1Text, const std::string &button2Text,
                          const std::function<void(int)> &handler);
} // namespace ui

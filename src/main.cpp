#include "routine/init.h"
#include "ui/Reponses/Dialogs.h"
#include "logs/logger.h"

#include <thread>
#include <chrono>

void demoProgressLoading() {
    // Demo: Show progress dialog with stages
    std::vector<std::string> stages = {"Preparing", "Connecting", "Authenticating", "Downloading", "Processing", "Complete"};

    ui::setProgressDialogStages(stages);
    ui::showProgressDialog("NoriLoadingDemo", "Demonstrating progress stages...", "", ui::ProgressDialogFlag::StatusChain);

    logger::hint("Demo loading started with 6 stages");

    // Simulate stage progression every 1.5 seconds in background thread
    std::thread demoThread([stages]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            ui::activateProgressDialogStage();
            logger::debug(std::string("Demo stage: ") + std::to_string(i + 2) + "/6");
        }

        // Final close
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        ui::closeProgressDialog();
        logger::hint("Demo loading completed");
    });

    demoThread.detach();  // Run in background
}

int main(int argc, char *argv[]) {
    // Uncomment to run demo loading:
    demoProgressLoading();

    const auto result = init::_init(argc, argv);
    return result;
}



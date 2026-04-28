#include "Dialogs.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#include "../../logs/logger.h"

#include <utility>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace {
NSPanel *gProgressPanel = nil;
NSProgressIndicator *gProgressBar = nil;
NSTextField *gProgressDescriptionLabel = nil;
NSTextField *gProgressStatusLabel = nil;
NSTextField *gProgressElapsedLabel = nil;
dispatch_source_t gProgressFunctionTimer = nil;
dispatch_source_t gProgressElapsedTimer = nil;
std::function<void()> gProgressFunctionHandler = nullptr;
ui::ProgressDialogFlag gProgressDialogFlag = ui::ProgressDialogFlag::Continuous;
std::vector<std::string> gProgressStages;
std::size_t gProgressStageIndex = 0;
std::chrono::steady_clock::time_point gProgressStartTime;

template <typename Block>
void runOnMainThreadSync(Block block) {
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

NSTextField *createDescriptionLabel(const std::string &text) {
    NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 350, 40)];
    [label setStringValue:[NSString stringWithUTF8String:text.c_str()]];
    [label setEditable:NO];
    [label setBordered:NO];
    [label setBezeled:NO];
    [label setDrawsBackground:NO];
    [label setFont:[NSFont systemFontOfSize:13]];
    [label setTextColor:[NSColor secondaryLabelColor]];
    [label setMaximumNumberOfLines:2];
    [label setLineBreakMode:NSLineBreakByWordWrapping];
    [label setAlignment:NSTextAlignmentCenter];
    return label;
}

NSTextField *createStatusLabel(const std::string &text) {
    NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 350, 20)];
    [label setStringValue:[NSString stringWithUTF8String:text.c_str()]];
    [label setEditable:NO];
    [label setBordered:NO];
    [label setBezeled:NO];
    [label setDrawsBackground:NO];
    [label setFont:[NSFont boldSystemFontOfSize:12]];
    [label setTextColor:[NSColor labelColor]];
    [label setAlignment:NSTextAlignmentCenter];
    return label;
}

void stopProgressFunctionTimer() {
    if (gProgressFunctionTimer) {
        dispatch_source_cancel(gProgressFunctionTimer);
        gProgressFunctionTimer = nil;
    }
}

void stopElapsedTimer() {
    if (gProgressElapsedTimer) {
        dispatch_source_cancel(gProgressElapsedTimer);
        gProgressElapsedTimer = nil;
    }
}

void startProgressFunctionTimerIfNeeded() {
    if (gProgressFunctionTimer || !gProgressPanel || !gProgressFunctionHandler) {
        return;
    }

    gProgressFunctionTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    if (!gProgressFunctionTimer) {
        return;
    }

    dispatch_source_set_timer(gProgressFunctionTimer,
                              dispatch_time(DISPATCH_TIME_NOW, 0),
                              120 * NSEC_PER_MSEC,
                              20 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(gProgressFunctionTimer, ^{
        if (gProgressPanel && gProgressFunctionHandler) {
            gProgressFunctionHandler();
        }
    });
    dispatch_resume(gProgressFunctionTimer);
}

void startElapsedTimerIfNeeded() {
    if (gProgressElapsedTimer || !gProgressPanel) return;
    gProgressStartTime = std::chrono::steady_clock::now();
    gProgressElapsedTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    if (!gProgressElapsedTimer) return;
    dispatch_source_set_timer(gProgressElapsedTimer, dispatch_time(DISPATCH_TIME_NOW, 0), 1 * NSEC_PER_SEC, 200 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(gProgressElapsedTimer, ^{
        if (!gProgressElapsedLabel) return;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - gProgressStartTime).count();
        int minutes = static_cast<int>(elapsed / 60);
        int seconds = static_cast<int>(elapsed % 60);
        std::ostringstream ss;
        ss << std::setfill('0') << std::setw(2) << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
        std::string s = std::string("Elapsed: ") + ss.str();
        runOnMainThreadSync(^{ [gProgressElapsedLabel setStringValue:[NSString stringWithUTF8String:s.c_str()]]; });
    });
    dispatch_resume(gProgressElapsedTimer);
}

void updateStageLabel() {
    if (!gProgressStatusLabel) {
        return;
    }

    if (gProgressDialogFlag != ui::ProgressDialogFlag::StatusChain) {
        [gProgressStatusLabel setHidden:YES];
        return;
    }

    [gProgressStatusLabel setHidden:NO];

    std::string text;
    if (gProgressStages.empty()) {
        text = "Waiting for completion...";
    } else {
        const std::size_t index = gProgressStageIndex < gProgressStages.size() ? gProgressStageIndex : gProgressStages.size() - 1;
        text = std::to_string(index + 1) + "/" + std::to_string(gProgressStages.size()) + " - " + gProgressStages[index];
    }

    [gProgressStatusLabel setStringValue:[NSString stringWithUTF8String:text.c_str()]];
}

void clearProgressState() {
    stopProgressFunctionTimer();
    stopElapsedTimer();
    gProgressPanel = nil;
    gProgressBar = nil;
    gProgressDescriptionLabel = nil;
    gProgressStatusLabel = nil;
    gProgressElapsedLabel = nil;
    gProgressDialogFlag = ui::ProgressDialogFlag::Continuous;
    gProgressStages.clear();
    gProgressStageIndex = 0;
    gProgressFunctionHandler = nullptr;
}
} // namespace

namespace ui {

void functionProgressDialog(const std::function<void()> &handler) {
    auto handlerCopy = handler;
    runOnMainThreadSync(^{
        gProgressFunctionHandler = handlerCopy;
        if (!gProgressFunctionHandler) {
            stopProgressFunctionTimer();
            return;
        }
        startProgressFunctionTimerIfNeeded();
    });
}

void setProgressDialogStages(const std::vector<std::string> &stages) {
    auto stagesCopy = stages;
    runOnMainThreadSync(^{
        gProgressStages = stagesCopy;
        gProgressStageIndex = 0;
        updateStageLabel();
    });
}

void showProgressDialog(const std::string &message, const std::string &description, const std::string &iconPath,
                        ProgressDialogFlag flag) {
    auto messageCopy = message;
    auto descriptionCopy = description;
    auto iconPathCopy = iconPath;

    runOnMainThreadSync(^{
        if (gProgressPanel) {
            return;
        }

        gProgressDialogFlag = flag;
        gProgressStageIndex = 0;

        // Create a custom panel (no buttons)
        NSPanel *panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 420, 320)
                                                      styleMask:(NSTitledWindowMask | NSClosableWindowMask)
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
        [panel setLevel:NSModalPanelWindowLevel];
        [panel setTitle:[NSString stringWithUTF8String:messageCopy.c_str()]];
        [panel setReleasedWhenClosed:NO];
        [panel setMovable:YES];
        [panel setMovableByWindowBackground:YES];
        [panel center];

        NSView *contentView = [[NSView alloc] initWithFrame:[panel contentRectForFrameRect:[panel frame]]];
        [contentView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

        // Icon (if provided)
        if (!iconPathCopy.empty()) {
            NSImage *icon = [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:iconPathCopy.c_str()]];
            if (icon) {
                NSImageView *iconView = [[NSImageView alloc] initWithFrame:NSMakeRect(160, 240, 100, 60)];
                [iconView setImage:icon];
                [iconView setImageScaling:NSImageScaleProportionallyUpOrDown];
                [contentView addSubview:iconView];
            }
        }

        // Description label
        gProgressDescriptionLabel = createDescriptionLabel(descriptionCopy);
        [gProgressDescriptionLabel setFrame:NSMakeRect(35, 180, 350, 40)];
        [contentView addSubview:gProgressDescriptionLabel];

        // Status label (for StatusChain mode)
        gProgressStatusLabel = createStatusLabel("");
        [gProgressStatusLabel setFrame:NSMakeRect(35, 150, 350, 20)];
        [contentView addSubview:gProgressStatusLabel];

        // Elapsed label
        gProgressElapsedLabel = createStatusLabel("Elapsed: 00:00");
        [gProgressElapsedLabel setFont:[NSFont systemFontOfSize:11]];
        [gProgressElapsedLabel setFrame:NSMakeRect(35, 120, 350, 18)];
        [contentView addSubview:gProgressElapsedLabel];

        // Progress bar (spinner or determinate)
        gProgressBar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(160, 40, 100, 100)];
        [gProgressBar setIndeterminate:YES];
        [gProgressBar setStyle:NSProgressIndicatorSpinnerStyle];
        [gProgressBar setControlSize:NSControlSizeRegular];
        [gProgressBar setDisplayedWhenStopped:YES];
        [gProgressBar startAnimation:nil];
        [contentView addSubview:gProgressBar];

        if (flag == ProgressDialogFlag::StatusChain) {
            updateStageLabel();
        } else {
            [gProgressStatusLabel setHidden:YES];
        }

        [panel setContentView:contentView];
        gProgressPanel = panel;

        [NSApp activateIgnoringOtherApps:YES];
        [panel makeKeyAndOrderFront:nil];

        // Give AppKit one run-loop tick so the dialog can render before the request continues.
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];

        startProgressFunctionTimerIfNeeded();
        startElapsedTimerIfNeeded();
    });
}

void activateProgressDialogStage() {
    runOnMainThreadSync(^{
        if (!gProgressPanel || gProgressDialogFlag != ProgressDialogFlag::StatusChain) {
            return;
        }

        if (!gProgressStages.empty() && gProgressStageIndex + 1 < gProgressStages.size()) {
            ++gProgressStageIndex;
        }

        updateStageLabel();
    });
}

void updateProgress(double fraction) {
    const double clampedFraction = fraction < 0.0 ? 0.0 : (fraction > 1.0 ? 1.0 : fraction);
    runOnMainThreadSync(^{
        if (!gProgressBar) return;
        // If caller provides determinate updates, switch to determinate mode
        if ([gProgressBar isIndeterminate]) {
            [gProgressBar stopAnimation:nil];
            [gProgressBar setIndeterminate:NO];
            [gProgressBar setStyle:NSProgressIndicatorBarStyle];
            [gProgressBar setMinValue:0.0];
            [gProgressBar setMaxValue:1.0];
        }
        [gProgressBar setDoubleValue:clampedFraction];
    });
}

void closeProgressDialog() {
    runOnMainThreadSync(^{
        if (!gProgressPanel) {
            return;
        }

        stopProgressFunctionTimer();
        stopElapsedTimer();

        if ([gProgressBar isIndeterminate]) {
            [gProgressBar stopAnimation:nil];
        }

        [gProgressPanel orderOut:nil];
        clearProgressState();
    });
}

void showButtonDialog(const std::string &message, const std::string &description, const std::string &iconPath,
                      const std::string &button1Text, const std::string &button2Text,
                      const std::function<void(int)> &handler) {
    runOnMainThreadSync(^{
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSAlertStyleInformational];
        [alert setMessageText:[NSString stringWithUTF8String:message.c_str()]];
        [alert setAccessoryView:createDescriptionLabel(description)];

        if (!iconPath.empty()) {
            NSImage *icon = [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:iconPath.c_str()]];
            if (icon) {
                [alert setIcon:icon];
            }
        }

        [alert addButtonWithTitle:[NSString stringWithUTF8String:button1Text.c_str()]];
        if (!button2Text.empty()) {
            [alert addButtonWithTitle:[NSString stringWithUTF8String:button2Text.c_str()]];
        }

        NSModalResponse response = [alert runModal];
        if (handler) {
            int buttonIndex = 0;
            if (response == NSAlertFirstButtonReturn) {
                buttonIndex = 1;
            } else if (response == NSAlertSecondButtonReturn) {
                buttonIndex = 2;
            }
            handler(buttonIndex);
        }
    });
}

} // namespace ui

#else
namespace ui {
void functionProgressDialog(const std::function<void()> &) {}
void setProgressDialogStages(const std::vector<std::string> &) {}
void activateProgressDialogStage() {}
void showProgressDialog(const std::string &, const std::string &, const std::string &, ProgressDialogFlag) {}
void updateProgress(double) {}
void closeProgressDialog() {}
void showButtonDialog(const std::string &, const std::string &, const std::string &, const std::string &, const std::string &, const std::function<void(int)> &) {}
} // namespace ui
#endif


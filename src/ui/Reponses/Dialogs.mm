#include "Dialogs.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

namespace {
NSAlert* gProgressAlert = nil;
NSProgressIndicator* gProgressBar = nil;

// Creates a styled text field for the description
NSTextField* createDescriptionLabel(const std::string& text) {
    NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 280, 40)];
    [label setStringValue:[NSString stringWithUTF8String:text.c_str()]];
    [label setEditable:NO];
    [label setBordered:NO];
    [label setBezeled:NO];
    [label setDrawsBackground:NO];
    [label setFont:[NSFont systemFontOfSize:11]];
    [label setTextColor:[NSColor secondaryLabelColor]];
    [label setMaximumNumberOfLines:2];
    [label setLineBreakMode:NSLineBreakByWordWrapping];
    return label;
}

}

namespace ui {

void showProgressDialog(const std::string& message, const std::string& description, const std::string& iconPath) {
    if (gProgressAlert) {
        return;
    }

    gProgressAlert = [[NSAlert alloc] init];
    [gProgressAlert setAlertStyle:NSAlertStyleInformational];
    [gProgressAlert setMessageText:[NSString stringWithUTF8String:message.c_str()]];

    if (!iconPath.empty()) {
        NSImage* icon = [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:iconPath.c_str()]];
        if (icon) {
            [gProgressAlert setIcon:icon];
        }
    }

    // Create a container view for the description and progress bar
    NSView* accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 280, 60)];

    NSTextField* descriptionLabel = createDescriptionLabel(description);
    [descriptionLabel setFrame:NSMakeRect(0, 25, 280, 35)];
    [accessoryView addSubview:descriptionLabel];

    gProgressBar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, 280, 20)];
    [gProgressBar setIndeterminate:NO];
    [gProgressBar setMinValue:0.0];
    [gProgressBar setMaxValue:1.0];
    [gProgressBar setDoubleValue:0.0];
    [accessoryView addSubview:gProgressBar];

    [gProgressAlert setAccessoryView:accessoryView];

    [gProgressAlert beginSheetModalForWindow:[NSApp keyWindow] completionHandler:nil];
}

void updateProgress(double fraction) {
    if (gProgressBar) {
        [gProgressBar setDoubleValue:fraction];
    }
}

void closeProgressDialog() {
    if (gProgressAlert) {
        [[gProgressAlert window] orderOut:nil];
        gProgressAlert = nil;
        gProgressBar = nil;
    }
}

void showButtonDialog(const std::string& message, const std::string& description, const std::string& iconPath, const std::string& button1Text, const std::string& button2Text, std::function<void(int)> handler) {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setAlertStyle:NSAlertStyleInformational];
    [alert setMessageText:[NSString stringWithUTF8String:message.c_str()]];

    // Replace informativeText with a custom styled label
    [alert setAccessoryView:createDescriptionLabel(description)];

    if (!iconPath.empty()) {
        NSImage* icon = [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:iconPath.c_str()]];
        if (icon) {
            [alert setIcon:icon];
        }
    }

    [alert addButtonWithTitle:[NSString stringWithUTF8String:button1Text.c_str()]];
    if (!button2Text.empty()) {
        [alert addButtonWithTitle:[NSString stringWithUTF8String:button2Text.c_str()]];
    }

    dispatch_async(dispatch_get_main_queue(), ^{
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
void showProgressDialog(const std::string&, const std::string&, const std::string&) {}
void updateProgress(double) {}
void closeProgressDialog() {}
void showButtonDialog(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, std::function<void(int)>) {}
} // namespace ui
#endif

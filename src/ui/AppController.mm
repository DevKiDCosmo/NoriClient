#include "AppController.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>

#include <utility>

#include "../logs/logger.h"

namespace {
std::string gVersion;
std::string gPortsSummary;
std::string gAppIconPath;
ui::IncomingUrlHandler gIncomingUrlHandler;
id gDelegate = nil;
NSStatusItem *gStatusItem = nil;
NSPanel *gInfoPanel = nil;
NSTextField *gInfoText = nil;
NSImageView *gInfoIconView = nil;

NSImage *loadIcon(const std::string &path) {
    if (!path.empty()) {
        NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
        if (nsPath != nil && [[NSFileManager defaultManager] fileExistsAtPath:nsPath]) {
            NSImage *image = [[NSImage alloc] initWithContentsOfFile:nsPath];
            if (image != nil) {
                return image;
            }
        }
    }

    NSImage *lockIcon = [NSImage imageNamed:NSImageNameLockLockedTemplate];
    if (lockIcon != nil) {
        return lockIcon;
    }

    return [NSApp applicationIconImage];
}

NSString *toNSString(const std::string &text) {
    NSString *result = [NSString stringWithUTF8String:text.c_str()];
    if (result == nil) {
        result = @"";
    }
    return result;
}

void updateInfoPanelContent() {
    if (gInfoPanel == nil) {
        return;
    }

    NSString *version = toNSString(gVersion);
    NSString *ports = toNSString(gPortsSummary);
    NSString *body = [NSString stringWithFormat:@"Version: %@\nOpen ports / handlers:\n%@\n\nnori-slk://host -> authentication\nnori-api:// -> callback / response", version, ports];
    [gInfoText setStringValue:body];

    NSImage *icon = loadIcon(gAppIconPath);
    if (icon != nil) {
        [gInfoIconView setImage:icon];
    }
}

void ensureInfoPanel() {
    if (gInfoPanel != nil) {
        updateInfoPanelContent();
        return;
    }

    NSRect frame = NSMakeRect(0, 0, 360, 180);
    gInfoPanel = [[NSPanel alloc] initWithContentRect:frame
                                             styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
    [gInfoPanel setTitle:@"NoriID"];
    [gInfoPanel setFloatingPanel:YES];
    [gInfoPanel setHidesOnDeactivate:YES];
    [gInfoPanel setLevel:NSStatusWindowLevel];

    NSView *content = [gInfoPanel contentView];

    gInfoIconView = [[NSImageView alloc] initWithFrame:NSMakeRect(20, 112, 48, 48)];
    [gInfoIconView setImage:loadIcon(gAppIconPath)];
    [gInfoIconView setImageScaling:NSImageScaleProportionallyUpOrDown];
    [content addSubview:gInfoIconView];

    NSTextField *title = [[NSTextField alloc] initWithFrame:NSMakeRect(84, 116, 240, 24)];
    [title setStringValue:@"NoriID background service"];
    [title setEditable:NO];
    [title setBordered:NO];
    [title setBezeled:NO];
    [title setDrawsBackground:NO];
    [title setFont:[NSFont boldSystemFontOfSize:14]];
    [content addSubview:title];

    gInfoText = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 18, 320, 90)];
    [gInfoText setEditable:NO];
    [gInfoText setBordered:NO];
    [gInfoText setBezeled:NO];
    [gInfoText setDrawsBackground:NO];
    [gInfoText setFont:[NSFont systemFontOfSize:12]];
    [gInfoText setLineBreakMode:NSLineBreakByWordWrapping];
    [gInfoText setUsesSingleLineMode:NO];
    [gInfoText setMaximumNumberOfLines:0];
    [content addSubview:gInfoText];

    updateInfoPanelContent();
}

void showInfoPanel() {
    ensureInfoPanel();
    [gInfoPanel center];
    [gInfoPanel makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void setupStatusItem() {
    if (gStatusItem != nil) {
        return;
    }

    gStatusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    NSStatusBarButton *button = [gStatusItem button];
    [button setImage:[NSImage imageNamed:NSImageNameLockLockedTemplate]];
    [button setImagePosition:NSImageOnly];
    [button setTarget:gDelegate];
    [button setAction:@selector(toggleInfoWindow:)];
    [button setToolTip:@"NoriID background service"];
}
} // namespace

@interface NoriAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation NoriAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    setupStatusItem();
}

- (void)application:(NSApplication *)application openURLs:(NSArray<NSURL *> *)urls {
    (void)application;
    for (NSURL *url in urls) {
        if (url == nil) {
            continue;
        }
        if (gIncomingUrlHandler) {
            gIncomingUrlHandler([[url absoluteString] UTF8String]);
        } else {
            logger::warning("No URL handler installed for: " + std::string([[url absoluteString] UTF8String]));
        }
    }
}

- (void)toggleInfoWindow:(id)sender {
    (void)sender;
    showInfoPanel();
}

@end

namespace ui {

void installAppController(const std::string &version,
                          const std::string &portsSummary,
                          const std::string &appIconPath,
                          IncomingUrlHandler handler) {
    gVersion = version;
    gPortsSummary = portsSummary;
    gAppIconPath = appIconPath;
    gIncomingUrlHandler = std::move(handler);

    [NSApplication sharedApplication];
    gDelegate = [[NoriAppDelegate alloc] init];
    [NSApp setDelegate:gDelegate];
    [NSApp finishLaunching];
    setupStatusItem();
}

void runApplication() {
    [NSApp run];
}

void showInfoWindow() {
    showInfoPanel();
}

} // namespace ui

#else
namespace ui {
void installAppController(const std::string &, const std::string &, const std::string &, IncomingUrlHandler) {}
void runApplication() {}
void showInfoWindow() {}
} // namespace ui
#endif


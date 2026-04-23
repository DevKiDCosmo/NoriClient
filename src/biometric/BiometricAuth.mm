#include "BiometricAuth.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <LocalAuthentication/LocalAuthentication.h>
#include <dispatch/dispatch.h>

#include "../logs/logger.h"

namespace biometric {

namespace {
constexpr const char *kFallbackId = "demo-id";
constexpr const char *kFallbackPassword = "demo-password";

std::string nsErrorToString(NSError *error) {
    if (error == nil) {
        logger::fatal("Failed to get error description from NSError");
        return "Unknown biometric error";
    }

    NSString *description = [error localizedDescription];
    if (description == nil) {
        logger::fatal("Failed to get error description from NSError");
        return "Unknown biometric error";
    }

    return {[description UTF8String]};
}

template <typename F>
auto runOnMainThread(F &&block) {
    using ReturnType = decltype(block());

    if ([NSThread isMainThread]) {
        return block();
    }

    __block ReturnType result{};
    dispatch_sync(dispatch_get_main_queue(), ^{
      result = block();
    });
    return result;
}

void prepareDialogHost() {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp activateIgnoringOtherApps:YES];
}

bool askForPasswordFallback() {
    return runOnMainThread([] {
      prepareDialogHost();

      NSAlert *alert = [[NSAlert alloc] init];
      [alert setMessageText:@"Biometric authentication failed"];
      [alert setInformativeText:@"Do you want to continue with ID and password?"];
      [alert addButtonWithTitle:@"Use Password"];
      [alert addButtonWithTitle:@"Cancel"];

      const NSModalResponse response = [alert runModal];
      return response == NSAlertFirstButtonReturn;
    });
}

bool promptCustomCredentials(std::string &errorMessage) {
    return runOnMainThread([&errorMessage] {
      prepareDialogHost();

      NSAlert *alert = [[NSAlert alloc] init];
      [alert setMessageText:@"Password verification"];
      [alert setInformativeText:@"Enter ID and password to continue this request. Login via biometrics failed. The use of user password is prohibited."];
      [alert addButtonWithTitle:@"Login"];
      [alert addButtonWithTitle:@"Cancel"];

      NSView *container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 300, 52)];

      NSTextField *idLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 30, 70, 22)];
      [idLabel setStringValue:@"ID:"];
      [idLabel setEditable:NO];
      [idLabel setBordered:NO];
      [idLabel setBezeled:NO];
      [idLabel setDrawsBackground:NO];

      NSTextField *idField = [[NSTextField alloc] initWithFrame:NSMakeRect(80, 28, 220, 24)];

      NSTextField *passwordLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 2, 70, 22)];
      [passwordLabel setStringValue:@"Password:"];
      [passwordLabel setEditable:NO];
      [passwordLabel setBordered:NO];
      [passwordLabel setBezeled:NO];
      [passwordLabel setDrawsBackground:NO];

      NSSecureTextField *passwordField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(80, 0, 220, 24)];

      [container addSubview:idLabel];
      [container addSubview:idField];
      [container addSubview:passwordLabel];
      [container addSubview:passwordField];
      [alert setAccessoryView:container];

      const NSModalResponse response = [alert runModal];
      if (response != NSAlertFirstButtonReturn) {
          errorMessage = "Password authentication canceled.";
          return false;
      }

      const std::string inputId = [[idField stringValue] UTF8String] ? [[idField stringValue] UTF8String] : "";
      const std::string inputPassword = [[passwordField stringValue] UTF8String] ? [[passwordField stringValue] UTF8String] : "";

      if (inputId == kFallbackId && inputPassword == kFallbackPassword) {
          return true;
      }

      errorMessage = "Invalid ID or password.";
      return false;
    });
}
} // namespace

bool authorizeRequest(const std::string &reason, std::string &errorMessage) {
    @autoreleasepool {
        LAContext *context = [[LAContext alloc] init];
        NSError *policyError = nil;
        const LAPolicy policy = LAPolicyDeviceOwnerAuthenticationWithBiometrics;

        if (![context canEvaluatePolicy:policy error:&policyError]) {
            errorMessage = nsErrorToString(policyError);
            return false;
        }

        NSString *localizedReason = [NSString stringWithUTF8String:reason.c_str()];
        if (localizedReason == nil || [localizedReason length] == 0) {
            localizedReason = @"Authenticate to continue.";
        }

        __block BOOL success = NO;
        __block std::string callbackErrorMessage;
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

        [context evaluatePolicy:policy
                localizedReason:localizedReason
                          reply:^(BOOL evaluated, NSError *_Nullable evaluateError) {
                              success = evaluated;
                              if (!evaluated) {
                                  callbackErrorMessage = nsErrorToString(evaluateError);
                              }
                              dispatch_semaphore_signal(semaphore);
                          }];

        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

        if (!success) {
            if (askForPasswordFallback()) {
                if (promptCustomCredentials(errorMessage)) {
                    logger::system("Password fallback authentication successful.");
                    return true;
                }
                logger::warning("Password fallback authentication failed.");
                return false;
            }

            errorMessage = callbackErrorMessage.empty() ? "Biometric authentication canceled." : callbackErrorMessage;
            return false;
        }

        return true;
    }
}

} // namespace biometric
#endif


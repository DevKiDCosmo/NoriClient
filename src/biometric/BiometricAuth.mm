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
bool gUiHostInitialized = false;

NSImage *resolveDialogIcon(const std::string &dialogIconPath);

void ensureAppInitialized() {
    if (!gUiHostInitialized) {
        [NSApplication sharedApplication];
        [[NSProcessInfo processInfo] setProcessName:@"NoriID"];
        if ([NSApp respondsToSelector:@selector(finishLaunching)]) {
            [NSApp finishLaunching];
        }
        if ([NSWindow respondsToSelector:@selector(setAllowsAutomaticWindowTabbing:)]) {
            [NSWindow setAllowsAutomaticWindowTabbing:NO];
        }
        gUiHostInitialized = true;
    }
}

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

void prepareAuthDialogHost() {
    ensureAppInitialized();
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp activateIgnoringOtherApps:YES];
}

void sendAuthUiToBackground() {
    if (NSApp != nil) {
        [NSApp hide:nil];
    }
}

void prepareResponseUiHost(const std::string &appIconPath) {
    ensureAppInitialized();
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSImage *appIcon = resolveDialogIcon(appIconPath);
    if (appIcon != nil) {
        [NSApp setApplicationIconImage:appIcon];
    }

    [NSApp activateIgnoringOtherApps:YES];
}

NSImage *resolveDialogIcon(const std::string &dialogIconPath) {
    if (!dialogIconPath.empty()) {
        NSString *path = [NSString stringWithUTF8String:dialogIconPath.c_str()];
        if (path != nil && [[NSFileManager defaultManager] fileExistsAtPath:path]) {
            NSImage *customIcon = [[NSImage alloc] initWithContentsOfFile:path];
            if (customIcon != nil) {
                return customIcon;
            }
        }
    }

    NSImage *lockIcon = [NSImage imageNamed:NSImageNameLockLockedTemplate];
    if (lockIcon != nil) {
        return lockIcon;
    }

    return [NSApp applicationIconImage];
}

bool waitForSemaphoreWithRunLoop(dispatch_semaphore_t semaphore, NSTimeInterval timeoutSeconds) {
    const NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeoutSeconds];
    while ([deadline timeIntervalSinceNow] > 0) {
        if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC)) == 0) {
            return true;
        }

        // Keep the main run loop responsive while waiting for async auth callbacks.
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }

    return false;
}

enum class CredentialsDialogAction {
    Success,
    Canceled,
    UseNative,
    Failed
};

bool isUserCanceledLaError(NSInteger code) {
    return code == LAErrorUserCancel || code == LAErrorSystemCancel || code == LAErrorAppCancel;
}

// Must be called from the main thread.
AuthResult authenticateWithNativeMacLogin() {
    NSCAssert([NSThread isMainThread], @"authenticateWithNativeMacLogin must be called on the main thread");
    prepareAuthDialogHost();

    LAContext *nativeContext = [[LAContext alloc] init];
    // Allow device password/PIN fallback in the native system dialog.
    nativeContext.localizedFallbackTitle = @"Use Password";

    NSError *policyError = nil;
    const LAPolicy nativePolicy = LAPolicyDeviceOwnerAuthentication;
    if (![nativeContext canEvaluatePolicy:nativePolicy error:&policyError]) {
        const std::string msg = nsErrorToString(policyError);
        logger::warning("Native macOS login unavailable: " + msg);
        return AuthResult::Failed;
    }

    __block BOOL success = NO;
    __block std::string callbackErrorMessage;
    __block NSInteger callbackErrorCode = 0;
    __block bool hasCallbackErrorCode = false;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [nativeContext evaluatePolicy:nativePolicy
                  localizedReason:@"Authenticate with native macOS login to continue."
                            reply:^(BOOL evaluated, NSError *_Nullable evaluateError) {
                                success = evaluated;
                                if (!evaluated) {
                                    callbackErrorMessage = nsErrorToString(evaluateError);
                                    if (evaluateError != nil &&
                                        [[evaluateError domain] isEqualToString:LAErrorDomain]) {
                                        callbackErrorCode = [evaluateError code];
                                        hasCallbackErrorCode = true;
                                    }
                                }
                                dispatch_semaphore_signal(semaphore);
                            }];

    if (!waitForSemaphoreWithRunLoop(semaphore, 45.0)) {
        logger::warning("Native macOS login timed out.");
        return AuthResult::Failed;
    }

    if (!success) {
        if (hasCallbackErrorCode && isUserCanceledLaError(callbackErrorCode)) {
            logger::information("Native macOS login canceled by user.");
            return AuthResult::Canceled;
        }
        const std::string msg = callbackErrorMessage.empty() ? "Native macOS login failed." : callbackErrorMessage;
        logger::warning("Native macOS login failed: " + msg);
        return AuthResult::Failed;
    }

    return AuthResult::Success;
}

CredentialsDialogAction promptCustomCredentials(bool debugMode, const std::string &dialogIconPath) {
    return runOnMainThread([debugMode, &dialogIconPath] {
      prepareAuthDialogHost();

       // TODO: Add disabled button if any field is empty. Also add error windows like "Invalid credentials" and "Server error, try again later" for better UX. For now, this is just a demo of custom credential dialog. Also add retry. If wrong credentials add new window with retry or cancel. Also add time for next login if logins seem suspicious.

      NSAlert *alert = [[NSAlert alloc] init];
      [alert setMessageText:@"Password verification"];
      [alert setInformativeText:@"Enter ID and password to continue this request. Login via biometrics failed. The use of user password is prohibited."];
      NSImage *dialogIcon = resolveDialogIcon(dialogIconPath);
      if (dialogIcon != nil) {
          [alert setIcon:dialogIcon];
      }
      [alert addButtonWithTitle:@"Login"];
      [alert addButtonWithTitle:@"Cancel"];
      if (debugMode) {
          [alert addButtonWithTitle:@"Login through Native"];
      }

      NSView *container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 300, 52)];

      NSTextField *idLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 30, 70, 22)];
      [idLabel setStringValue:@"ID:"];
      [idLabel setEditable:NO];
      [idLabel setBordered:NO];
      [idLabel setBezeled:NO];
      [idLabel setDrawsBackground:NO];

      NSTextField *idField = [[NSTextField alloc] initWithFrame:NSMakeRect(80, 28, 220, 24)];
      // [idField setStringValue:@"testuser"]; Adding placeholder later or not.

      NSTextField *passwordLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 2, 70, 22)];
      [passwordLabel setStringValue:@"Password:"];
      [passwordLabel setEditable:NO];
      [passwordLabel setBordered:NO];
      [passwordLabel setBezeled:NO];
      [passwordLabel setDrawsBackground:NO];

      NSSecureTextField *passwordField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(80, 0, 220, 24)];
      // [passwordField setStringValue:@"password"];

      [container addSubview:idLabel];
      [container addSubview:idField];
      [container addSubview:passwordLabel];
      [container addSubview:passwordField];
      [alert setAccessoryView:container];

      const NSModalResponse response = [alert runModal];
      if (debugMode && response == NSAlertThirdButtonReturn) {
          return CredentialsDialogAction::UseNative;
      }

       if (response == NSAlertFirstButtonReturn) {
           // TODO: HARD coded value are used for demonstration purposes
           const std::string inputId = [[idField stringValue] UTF8String] ? [[idField stringValue] UTF8String] : "";
           const std::string inputPassword = [[passwordField stringValue] UTF8String] ? [[passwordField stringValue] UTF8String] : "";
            // logger::debug("Typen: " + inputId + " " + inputPassword); TODO: Adding to logger environment declaration system to global access.

           if ([idField.stringValue isEqualToString:@"123"] && [passwordField.stringValue isEqualToString:@"123"]) {
               logger::system("Custom credential verification successful (debug mode).");
               return CredentialsDialogAction::Success;
           }
       }

      if (response != NSAlertFirstButtonReturn) {
          return CredentialsDialogAction::Canceled;
      }

      // Credential validation requires server-side implementation.
      // Client-side credential checks have been removed for security.
      logger::warning("Credential fallback is not available: server-side validation required.");
      return CredentialsDialogAction::Failed;
    });
}
} // namespace

void initializeUiHost() {
    runOnMainThread([] {
      prepareAuthDialogHost();
      return true;
    });
}

void showJsonResponseWindow(const std::string &jsonText, const std::string &appIconPath) {
    runOnMainThread([&jsonText, &appIconPath] {
      prepareResponseUiHost(appIconPath);

      NSAlert *alert = [[NSAlert alloc] init];
      [alert setMessageText:@"JSON Response"];
      [alert setInformativeText:@"Request completed successfully."];
      NSImage *dialogIcon = resolveDialogIcon(appIconPath);
      if (dialogIcon != nil) {
          [alert setIcon:dialogIcon];
      }
        // TODO: Close button don't actual remove icon from taskbar.
      [alert addButtonWithTitle:@"Close"];

      NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 560, 300)];
      [scrollView setHasVerticalScroller:YES];
      [scrollView setHasHorizontalScroller:YES];
      [scrollView setAutohidesScrollers:YES];

      NSTextView *textView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 560, 300)];
      [textView setEditable:NO];
      [textView setSelectable:YES];
      [textView setFont:[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular]];
      NSString *jsonNSString = [NSString stringWithUTF8String:jsonText.c_str()];
      if (jsonNSString == nil) {
          jsonNSString = @"(Invalid UTF-8 response body)";
      }
      [textView setString:jsonNSString];
      [scrollView setDocumentView:textView];
      [alert setAccessoryView:scrollView];

      [alert runModal];
      return true;
    });
}

AuthResult authorizeRequest(const std::string &reason, bool debugMode, const std::string &dialogIconPath) {
    @autoreleasepool {
        LAContext *context = [[LAContext alloc] init];
        NSError *policyError = nil;
        const LAPolicy policy = LAPolicyDeviceOwnerAuthenticationWithBiometrics; // ! Don't change to LAPolicyDeviceOwnerAuthentication.

        if (![context canEvaluatePolicy:policy error:&policyError]) {
            const std::string msg = nsErrorToString(policyError);
            logger::warning("Biometric authentication unavailable: " + msg);
            return AuthResult::Failed;
        }

        NSString *localizedReason = [NSString stringWithUTF8String:reason.c_str()];
        if (localizedReason == nil || [localizedReason length] == 0) {
            localizedReason = @"Authenticate to continue.";
        }

        __block BOOL success = NO;
        __block std::string callbackErrorMessage;
        __block NSInteger callbackErrorCode = 0;
        __block bool hasCallbackErrorCode = false;
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

        [context evaluatePolicy:policy
                localizedReason:localizedReason
                          reply:^(BOOL evaluated, NSError *_Nullable evaluateError) {
                              success = evaluated;
                              if (!evaluated) {
                                  callbackErrorMessage = nsErrorToString(evaluateError);
                                  if (evaluateError != nil &&
                                      [[evaluateError domain] isEqualToString:LAErrorDomain]) {
                                      callbackErrorCode = [evaluateError code];
                                      hasCallbackErrorCode = true;
                                  }
                              }
                              dispatch_semaphore_signal(semaphore);
                          }];

        if (!waitForSemaphoreWithRunLoop(semaphore, 30.0)) {
            logger::warning("Biometric authentication timed out.");
            return AuthResult::Failed;
        }

        if (!success) {
            if (hasCallbackErrorCode && isUserCanceledLaError(callbackErrorCode)) {
                logger::information("Biometric authentication canceled by user.");
                runOnMainThread([] {
                  sendAuthUiToBackground();
                  return true;
                });
                return AuthResult::Canceled;
            }

            logger::warning("Biometric authentication failed: " + callbackErrorMessage);

            // !! This is the ID call goto.
            const CredentialsDialogAction action = promptCustomCredentials(debugMode, dialogIconPath);

            if (action == CredentialsDialogAction::UseNative) {
                const AuthResult nativeResult = authenticateWithNativeMacLogin();
                if (nativeResult == AuthResult::Success) {
                    logger::system("Native macOS login successful.");
                }
                runOnMainThread([] {
                  sendAuthUiToBackground();
                  return true;
                });
                return nativeResult;
            }

            if (action == CredentialsDialogAction::Success) {
                runOnMainThread([] {
                    sendAuthUiToBackground();
                    return true;
                });
                return AuthResult::Success;
            }

            runOnMainThread([] {
              sendAuthUiToBackground();
              return true;
            });

            if (action == CredentialsDialogAction::Canceled) {
                return AuthResult::Canceled;
            }

            // CredentialsDialogAction::Failed
            return AuthResult::Failed;
        }

        runOnMainThread([] {
          sendAuthUiToBackground();
          return true;
        });

        return AuthResult::Success;
    }
}

} // namespace biometric
#endif

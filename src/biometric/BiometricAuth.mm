#include "BiometricAuth.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#include <Foundation/Foundation.h>
#include <LocalAuthentication/LocalAuthentication.h>
#include <dispatch/dispatch.h>

#include "../logs/logger.h"

namespace biometric {

namespace {
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
            errorMessage = callbackErrorMessage.empty() ? "Biometric authentication canceled." : callbackErrorMessage;
            return false;
        }

        return true;
    }
}

} // namespace biometric
#endif


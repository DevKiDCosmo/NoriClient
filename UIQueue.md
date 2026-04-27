# UI Job Queue Implementation Plan

## Objective
To prevent UI-related crashes (e.g., `NSInternalInconsistencyException`) that occur when UI operations are attempted from background threads, and to ensure a responsive user interface while performing asynchronous tasks.

## Core Concept
The application's main thread is the *only* thread allowed to interact with the UI framework (AppKit on macOS, Android UI toolkit on Android). When a background thread needs to update the UI or display a dialog, it must *dispatch* a "UI job" back to the main thread for execution.

## Components

1.  **Main Thread**: The primary thread of the application, responsible for the event loop and all UI rendering.
2.  **Background Thread(s)**: Threads (like the one created for `processUri`) that perform long-running or blocking operations (e.g., network requests, heavy computations).
3.  **UI Job Queue**: A thread-safe queue where background threads can place UI-related tasks.
4.  **Dispatch Mechanism**: A way for the main thread to periodically check the UI Job Queue and execute any pending tasks. This often involves platform-specific APIs (e.g., `dispatch_async` on macOS, `Handler` on Android, `QMetaObject::invokeMethod` in Qt, or custom event loops).

## Implementation Steps

### Step 1: Define a UI Job Structure
Create a type that can encapsulate a UI operation. A `std::function` is a good choice for this.

```cpp
// In a common header, e.g., ui/UIQueue.h
#include <functional>
#include <queue>
#include <mutex>

namespace ui {

// Type definition for a UI job (a function to be executed on the main thread)
using UIJob = std::function<void()>;

// Thread-safe queue for UI jobs
class UIJobQueue {
public:
    void enqueue(UIJob job);
    std::optional<UIJob> dequeue();
    bool isEmpty() const;

private:
    std::queue<UIJob> queue_;
    mutable std::mutex mutex_;
};

// Global instance or singleton for the UI job queue
UIJobQueue& getUIJobQueue();

} // namespace ui
```

### Step 2: Implement Enqueue/Dequeue Logic
Implement the `enqueue`, `dequeue`, and `isEmpty` methods for `UIJobQueue`, ensuring thread safety with a `std::mutex`.

```cpp
// In ui/UIQueue.cpp
#include "UIQueue.h"

namespace ui {

void UIJobQueue::enqueue(UIJob job) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(job));
}

std::optional<UIJob> UIJobQueue::dequeue() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }
    UIJob job = std::move(queue_.front());
    queue_.pop();
    return job;
}

bool UIJobQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

UIJobQueue& getUIJobQueue() {
    static UIJobQueue instance;
    return instance;
}

} // namespace ui
```

### Step 3: Modify UI Calls in Background Threads
In `uriHandler::processUri` (and any other background threads), replace direct UI calls with enqueuing a UI job.

**Before (problematic):**
```cpp
biometric::showJsonResponseWindow("Failed to fetch nori-slk request.", config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath);
```

**After (solution):**
```cpp
// Capture necessary variables by value for the lambda
std::string message = "Failed to fetch nori-slk request.";
std::string iconPath = config.appIconPath.empty() ? config.dialogIconPath : config.appIconPath;

ui::getUIJobQueue().enqueue([message, iconPath]() {
    biometric::showJsonResponseWindow(message, iconPath);
});
```
This applies to all calls like `biometric::showJsonResponseWindow`, `ui::showInfoWindow`, etc.

### Step 4: Implement Main Thread Dispatch Loop
The main thread needs to regularly check the `UIJobQueue` and execute any pending jobs. This is the most platform-specific part.

**For macOS (using `dispatch_async`):**
You would typically have a function on the main thread that looks something like this:

```cpp
// In your main application loop or a timer callback on the main thread
void processPendingUIJobs() {
    while (auto job = ui::getUIJobQueue().dequeue()) {
        (*job)(); // Execute the UI job on the main thread
    }
    // Schedule this function to run again soon (e.g., using a timer or dispatch_after)
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.01 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        processPendingUIJobs();
    });
}

// Call this once at application startup on the main thread
// processPendingUIJobs();
```
The exact integration depends on how your macOS application's main loop is structured. If you're using a framework like Qt or similar, it would involve their specific signal/slot or event posting mechanisms.

## Considerations and Best Practices

*   **Thread Safety**: Ensure the `UIJobQueue` is fully thread-safe (using mutexes) as it will be accessed by multiple threads.
*   **Minimal UI Logic in Jobs**: Keep the UI jobs themselves as lightweight as possible. Their sole purpose should be to update the UI, not perform heavy computation.
*   **Error Handling**: Consider how errors within UI jobs should be handled. Uncaught exceptions in UI jobs executed on the main thread will still crash the application.
*   **Responsiveness**: The frequency at which the main thread checks the queue should be balanced. Too infrequent, and UI updates will be delayed; too frequent, and it might consume unnecessary CPU cycles. A timer-based approach (e.g., every 10-50ms) is often suitable.
*   **Context Capture**: When creating lambdas for UI jobs, carefully consider what variables need to be captured. Capture by value (`[var]`) for variables that might go out of scope on the background thread, or if their state might change before the UI job executes. Capture by reference (`[&var]`) is generally unsafe for background-to-main thread dispatch unless you can guarantee the lifetime of the referenced variable.
*   **Avoid Blocking Main Thread**: Ensure that the UI jobs themselves do not perform blocking operations, as this would defeat the purpose of keeping the main thread responsive.
*   **Platform-Specific Dispatch**: The exact mechanism for getting tasks onto the main thread will vary by platform and framework. For cross-platform C++ applications, you might need to abstract this with preprocessor directives or a common interface.

This plan provides a robust way to handle UI updates from background threads, ensuring stability and a smooth user experience.


```logs
/Users/duynamschlitz/CLionProjects/NoriID/cmake-build-debug/NoriID.app/Contents/MacOS/NoriID
[2026-04-27 12:55:42] [INFORMATION] NoriID client started.
[2026-04-27 12:55:42] [INIT] Configuration loaded successfully.
[2026-04-27 12:55:42] [INIT] - Server: auth.norigame.com
[2026-04-27 12:55:42] [INIT] - Port: 443
[2026-04-27 12:55:42] [INIT] - Biometric Required: true
[2026-04-27 12:55:42] [INIT] - Debug Mode: true
[2026-04-27 12:55:42] [INIT] - Dialog Icon: assets/nid.png
[2026-04-27 12:55:42] [INIT] - App Icon: assets/nid.png
[2026-04-27 12:55:42] [INIT] Version: v1.0
Version name: BUMBLEBEE
Copyright by DevKiD 2026
API endpoint: https://auth.norigame.com:443/api/v0.1
URI scheme: nori-slk://host[:port]/auth
Callback scheme: nori-api://
Auth scheme: nori-auth://
Request scheme: nori-request://

[2026-04-27 12:55:42] [IMPORTANT] Background service ready. Click the lock icon for details.
[2026-04-27 12:55:44] [API] Received nori-api URI: nori-api://127.0.0.1:48002/approve/b3591ae5170a05a71981c22a80cf5684%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-27 12:55:44] [DEBUG] Validate host: invalid
[2026-04-27 12:55:44] [DEBUG] Validate IPvX: valid
[2026-04-27 12:55:44] [SOCKET] nori-api path (safe decoded): /approve/b3591ae5170a05a71981c22a80cf5684%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-27 12:55:44] [DEBUG] Segment: approve
[2026-04-27 12:55:44] [DEBUG] Segment: b3591ae5170a05a71981c22a80cf5684
[2026-04-27 12:55:44] [API] Unknown nori-api path: /approve/b3591ae5170a05a71981c22a80cf5684%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-27 12:55:44] [API] Magic number found in nori-api path with purpose: 'approval'
[2026-04-27 12:55:45] [SYSTEM] Biometric authentication successful.
[2026-04-27 12:55:45] [API] Dispatching nori-api request to: 127.0.0.1:48002/approve/b3591ae5170a05a71981c22a80cf5684
[2026-04-27 12:55:45] [HINT] Authentication disabled.
*** Terminating app due to uncaught exception 'NSInternalInconsistencyException', reason: 'NSWindow should only be instantiated on the main thread!'
*** First throw call stack:
(
	0   CoreFoundation                      0x0000000183252bf0 __exceptionPreprocess + 176
	1   libobjc.A.dylib                     0x0000000182cde91c objc_exception_throw + 88
	2   CoreFoundation                      0x00000001832765c8 _CFBundleGetValueForInfoKey + 0
	3   AppKit                              0x00000001876202c4 -[NSWindow _initContent:styleMask:backing:defer:contentView:] + 260
	4   AppKit                              0x000000018776eff8 -[NSPanel _initContent:styleMask:backing:defer:contentView:] + 48
	5   AppKit                              0x00000001876201b4 -[NSWindow initWithContentRect:styleMask:backing:defer:] + 48
	6   AppKit                              0x000000018776efac -[NSPanel initWithContentRect:styleMask:backing:defer:] + 48
	7   AppKit                              0x000000018761f34c -[NSWindowTemplate nibInstantiate] + 216
	8   AppKit                              0x00000001875f1360 -[NSIBObjectData instantiateObject:] + 212
	9   AppKit                              0x00000001875f0d24 -[NSIBObjectData nibInstantiateWithOwner:options:topLevelObjects:] + 252
	10  AppKit                              0x00000001875e65bc loadNib + 340
	11  AppKit                              0x00000001875e5be0 +[NSBundle(NSNibLoading) _loadNibFile:nameTable:options:withZone:ownerBundle:] + 560
	12  AppKit                              0x00000001875e58e4 -[NSBundle(NSNibLoading) loadNibNamed:owner:topLevelObjects:] + 180
	13  AppKit                              0x00000001878a20fc -[NSAlert init] + 96
	14  NoriID                              0x000000010261841c _ZN2ui18showProgressDialogERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_ + 60
	15  NoriID                              0x00000001025be550 _ZN7network7request11MiniRequest8fetchRawERKNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEESA_RKNS0_13ProtocolChainE + 476
	16  NoriID                              0x00000001025be2d0 _ZN7network7request11MiniRequest5fetchERKNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEbbSA_RKNS0_13ProtocolChainE + 428
	17  NoriID                              0x0000000102612000 _ZZN10uriHandler10processUriERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEERKN3env9EnvConfigEENK3$_0clEv + 4664
	18  NoriID                              0x0000000102610d94 _ZNSt3__18__invokeB9nqe210106IZN10uriHandler10processUriERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEERKN3env9EnvConfigEE3$_0JEEEDTclclsr3stdE7declvalIT_EEspclsr3stdE7declvalIT0_EE	19  NoriID                              0x0000000102610d30 _ZNSt3__116__thread_executeB9nqe210106INS_10unique_ptrINS_15__thread_structENS_14default_deleteIS2_EEEEZN10uriHandler10processUriERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEERKN3e	20  NoriID                              0x00000001026108e0 _ZNSt3__114__thread_proxyB9nqe210106INS_5tupleIJNS_10unique_ptrINS_15__thread_structENS_14default_deleteIS3_EEEEZN10uriHandler10processUriERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIc	21  libsystem_pthread.dylib             0x0000000183127c58 _pthread_start + 136
	22  libsystem_pthread.dylib             0x0000000183122c1c thread_start + 8
)
libc++abi: terminating due to uncaught exception of type NSException

Process finished with exit code 134 (interrupted by signal 6:SIGABRT)


/Users/duynamschlitz/CLionProjects/NoriID/cmake-build-debug/NoriID.app/Contents/MacOS/NoriID
[2026-04-27 13:31:50] [INFORMATION] NoriID client started.
[2026-04-27 13:31:50] [INIT] Configuration loaded successfully.
[2026-04-27 13:31:50] [INIT] - Server: auth.norigame.com
[2026-04-27 13:31:50] [INIT] - Port: 443
[2026-04-27 13:31:50] [INIT] - Biometric Required: true
[2026-04-27 13:31:50] [INIT] - Debug Mode: true
[2026-04-27 13:31:50] [INIT] - Dialog Icon: assets/nid.png
[2026-04-27 13:31:50] [INIT] - App Icon: assets/nid.png
[2026-04-27 13:31:51] [INIT] Version: v1.0
Version name: BUMBLEBEE
Copyright by DevKiD 2026
API endpoint: https://auth.norigame.com:443/api/v0.1
URI scheme: nori-slk://host[:port]/auth
Callback scheme: nori-api://
Auth scheme: nori-auth://
Request scheme: nori-request://

[2026-04-27 13:31:51] [IMPORTANT] Background service ready. Click the lock icon for details.
[2026-04-27 13:31:56] [API] Received nori-api URI: nori-api://127.0.0.1:48002/approve/ee027ac922aabc97f742990e68d7f143%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-27 13:31:56] [DEBUG] Validate host: invalid
[2026-04-27 13:31:56] [DEBUG] Validate IPvX: valid
[2026-04-27 13:31:56] [SOCKET] nori-api path (safe decoded): /approve/ee027ac922aabc97f742990e68d7f143%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-27 13:31:56] [DEBUG] Segment: approve
[2026-04-27 13:31:56] [DEBUG] Segment: ee027ac922aabc97f742990e68d7f143
[2026-04-27 13:31:56] [API] Unknown nori-api path: /approve/ee027ac922aabc97f742990e68d7f143%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-27 13:31:56] [API] Magic number found in nori-api path with purpose: 'approval'
[2026-04-27 13:31:56] [SYSTEM] Biometric authentication successful.
[2026-04-27 13:31:56] [API] Dispatching nori-api request to: 127.0.0.1:48002/approve/ee027ac922aabc97f742990e68d7f143
[2026-04-27 13:31:56] [HINT] Authentication disabled.
*** Terminating app due to uncaught exception 'NSInternalInconsistencyException', reason: 'NSWindow should only be instantiated on the main thread!'
*** First throw call stack:
(
	0   CoreFoundation                      0x0000000183252bf0 __exceptionPreprocess + 176
	1   libobjc.A.dylib                     0x0000000182cde91c objc_exception_throw + 88
	2   CoreFoundation                      0x00000001832765c8 _CFBundleGetValueForInfoKey + 0
	3   AppKit                              0x00000001876202c4 -[NSWindow _initContent:styleMask:backing:defer:contentView:] + 260
	4   AppKit                              0x000000018776eff8 -[NSPanel _initContent:styleMask:backing:defer:contentView:] + 48
	5   AppKit                              0x00000001876201b4 -[NSWindow initWithContentRect:styleMask:backing:defer:] + 48
	6   AppKit                              0x000000018776efac -[NSPanel initWithContentRect:styleMask:backing:defer:] + 48
	7   AppKit                              0x000000018761f34c -[NSWindowTemplate nibInstantiate] + 216
	8   AppKit                              0x00000001875f1360 -[NSIBObjectData instantiateObject:] + 212
	9   AppKit                              0x00000001875f0d24 -[NSIBObjectData nibInstantiateWithOwner:options:topLevelObjects:] + 252
	10  AppKit                              0x00000001875e65bc loadNib + 340
	11  AppKit                              0x00000001875e5be0 +[NSBundle(NSNibLoading) _loadNibFile:nameTable:options:withZone:ownerBundle:] + 560
	12  AppKit                              0x00000001875e58e4 -[NSBundle(NSNibLoading) loadNibNamed:owner:topLevelObjects:] + 180
	13  AppKit                              0x00000001878a20fc -[NSAlert init] + 96
	14  NoriID                              0x000000010216c41c _ZN2ui18showProgressDialogERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_ + 60
	15  NoriID                              0x0000000102112550 _ZN7network7request11MiniRequest8fetchRawERKNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEESA_RKNS0_13ProtocolChainE + 476
	16  NoriID                              0x00000001021122d0 _ZN7network7request11MiniRequest5fetchERKNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEbbSA_RKNS0_13ProtocolChainE + 428
	17  NoriID                              0x0000000102166000 _ZZN10uriHandler10processUriERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEERKN3env9EnvConfigEENK3$_0clEv + 4664
	18  NoriID                              0x0000000102164d94 _ZNSt3__18__invokeB9nqe210106IZN10uriHandler10processUriERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEERKN3env9EnvConfigEE3$_0JEEEDTclclsr3stdE7declvalIT_EEspclsr3stdE7declvalIT0_EE	19  NoriID                              0x0000000102164d30 _ZNSt3__116__thread_executeB9nqe210106INS_10unique_ptrINS_15__thread_structENS_14default_deleteIS2_EEEEZN10uriHandler10processUriERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEERKN3e	20  NoriID                              0x00000001021648e0 _ZNSt3__114__thread_proxyB9nqe210106INS_5tupleIJNS_10unique_ptrINS_15__thread_structENS_14default_deleteIS3_EEEEZN10uriHandler10processUriERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIc	21  libsystem_pthread.dylib             0x0000000183127c58 _pthread_start + 136
	22  libsystem_pthread.dylib             0x0000000183122c1c thread_start + 8
)
libc++abi: terminating due to uncaught exception of type NSException

Process finished with exit code 134 (interrupted by signal 6:SIGABRT)

```
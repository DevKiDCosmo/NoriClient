# PLAN: Auth Stabilization and Native Login Constraints

## Goal
- Make biometric cancel a hard stop for the current login flow.
- End process with exit code `0` on explicit user cancel.
- Keep a clear audit trail with a fatal log on user cancel.
- Document why classic macOS system user/password login is unreliable in this app shape.

## Why native system user/password login is not reliable here

1. **CLI-style process + partial app lifecycle**
   - The binary is started like a command process, not a fully bundled Cocoa app with a normal lifecycle.
   - Some macOS auth UI paths assume a stable app bundle identity and full app/session context.

2. **Authorization Services behavior differs by context**
   - `AuthorizationCopyRights` may invoke SecurityAgent flows that are sensitive to session, TCC, process identity, and run loop state.
   - In non-standard contexts this can fail, freeze, or be terminated by the OS.

3. **Modern macOS prefers LocalAuthentication for user presence**
   - For app-level user verification, `LocalAuthentication` (`LAPolicyDeviceOwnerAuthentication`) is the stable path.
   - It supports biometric + device credential fallback in a single OS-managed flow.

4. **Not guaranteed to show classic username/password dialog**
   - macOS may present biometric/pin-first UI and only offer password fallback depending on device policy and settings.
   - Exact visuals are OS-controlled and should not be hard-required by app logic.

## Implemented behavior changes

1. **Cancel handling**
   - Detect `LAErrorUserCancel`, `LAErrorSystemCancel`, and `LAErrorAppCancel`.
   - Do **not** enter custom ID/password fallback after biometric cancel.

2. **Process termination semantics**
   - On auth cancel, log fatal and terminate normally with `return 0`.

3. **Fallback behavior scope**
   - Custom ID/password fallback is only reached for non-cancel biometric failures.
   - Debug-only native path stays separate and does not override cancel semantics.

## Next hardening steps

1. Remove hardcoded fallback credentials and move verification server-side.
2. Add rate limiting / lockout for fallback attempts.
3. Add explicit auth result enum instead of string matching for cancel semantics.
4. Convert to a true `.app` bundle if richer macOS-native auth UX is required.


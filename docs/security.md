# Security Notes

## Authentication Flow

1. The app tries biometric authentication first (`Touch ID`/`Face ID`) through LocalAuthentication.
2. If biometric auth fails or is canceled, the user can choose a password fallback dialog.
3. The password fallback now uses the current logged-in macOS user password prompt; the old fake `ID` / `Password` validation path was removed.
4. In debug mode only (`type=debug` in `data/.env`), a third action `Login through Native` is shown.
5. `Login through Native` uses macOS authorization services to prompt directly for the current user's password, without routing through biometrics again.
6. The auth dialog icon can be customized with `dialog_icon_path` in `data/.env`.
7. The background status-item / info window icon can be customized with `app_icon_path` in `data/.env`.

## Demo Fallback Credentials

The old hardcoded fallback credentials have been removed from the local client-side flow.

## Important Warnings

- Hardcoded credentials are **not used** in the current fallback path.
- `type=debug` must not be enabled in production, because it exposes extra local auth fallback behavior.
- Replace this with a server-side authentication flow (token-based, salted password hash verification).
- Never log passwords, raw secrets, or long-lived tokens.
- Keep TLS enabled for all API requests.

## Recommended Production Upgrade

- Move fallback authentication to backend verification.
- Use rate-limiting and lockout for repeated password failures.
- Store secrets in OS keychain or secure secret manager, not in source code.
- Keep biometric checks local and use signed server challenges for high-risk actions.


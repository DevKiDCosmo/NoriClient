# Security Notes

## Authentication Flow

1. The app tries biometric authentication first (`Touch ID`/`Face ID`) through LocalAuthentication.
2. If biometric auth fails or is canceled, the user can choose a password fallback dialog.
3. The password fallback uses a custom dialog with `ID` and `Password` fields.
4. In debug mode only (`type=debug` in `data/.env`), a third action `Login through Native` is shown.
5. `Login through Native` uses macOS `LocalAuthentication` (`DeviceOwnerAuthentication`) and can fall back to system password/PIN in the native prompt (debug only).
6. The fallback dialog icon can be customized with `dialog_icon_path` in `data/.env`.

## Demo Fallback Credentials

The current fallback credentials are hardcoded for development/demo purposes:

- ID: `demo-id`
- Password: `demo-password`

## Important Warnings

- Hardcoded credentials are **not secure** and must not be used in production.
- `type=debug` must not be enabled in production, because it exposes extra local auth fallback behavior.
- Replace this with a server-side authentication flow (token-based, salted password hash verification).
- Never log passwords, raw secrets, or long-lived tokens.
- Keep TLS enabled for all API requests.

## Recommended Production Upgrade

- Move fallback authentication to backend verification.
- Use rate-limiting and lockout for repeated password failures.
- Store secrets in OS keychain or secure secret manager, not in source code.
- Keep biometric checks local and use signed server challenges for high-risk actions.


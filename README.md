# NoriClient / NoriID

NoriID is a macOS background service with a menu-bar lock icon. It handles login requests and callback traffic over custom URI schemes.

## URI handlers

- `nori-slk://host` → starts authentication for the host/service.
- `nori-api://...` → callback/response handler.

## Background UI

- Only a lock icon is shown in the menu bar.
- Clicking the icon opens a small info panel with version and open handlers/ports.
- JSON responses are shown in a separate response window after a successful request.

## Icons

`src/nid.svg` is not used directly by macOS. Render it to a compatible PNG first:

```zsh
./scripts/render_icon.sh src/nid.svg data/nid.png
```

Then set in `data/.env`:

```dotenv
app_icon_path=data/nid.png
dialog_icon_path=
```

## Auth notes

- Biometrics are tried first.
- If biometrics fail, the app falls back to the current logged-in macOS user password prompt via Security.framework.
- The fallback is intentionally limited to the logged-in user path; no separate admin credential flow is used.



# Close should remove icon from task bar.
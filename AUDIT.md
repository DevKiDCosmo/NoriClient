# Initial Security Audit (v1)

Date: 2026-04-23
Scope: `src/main.cpp`, `src/biometric/BiometricAuth.mm`, `docs/security.md`

## Findings (ordered by severity)

### Critical
1. **Hardcoded credentials in client binary**
   - Location: `src/biometric/BiometricAuth.mm` (`kFallbackId`, `kFallbackPassword`)
   - Risk: Anyone with binary/source access can recover credentials; complete bypass of trust model.
   - Recommendation: Remove client-side credential checks. Validate credentials/challenges server-side.

### High
2. **Debug auth surface exposed through runtime config**
   - Location: `type=debug` in `data/.env`, consumed in `src/main.cpp`
   - Risk: Misconfiguration can expose extra local auth paths in production.
   - Recommendation: Gate debug auth with compile-time flag and CI policy, not only `.env`.

3. **Ambiguous auth-state handling via message strings**
   - Location: cancel handling and error propagation in `src/main.cpp`
   - Risk: String-based state detection can regress or be localized unexpectedly.
   - Recommendation: Use explicit typed auth result enum (`Success`, `Canceled`, `Denied`, `Error`).

### Medium
4. **No attempt throttling for fallback auth attempts**
   - Location: custom ID/password dialog flow
   - Risk: Local brute force attempts are possible.
   - Recommendation: Add local attempt cap + cool-down and server-side lockout.

5. **Potential sensitive data exposure in logs**
   - Location: generic logging paths around auth/network errors
   - Risk: Future changes may accidentally log secrets or token-like data.
   - Recommendation: Add structured redaction guard for auth/network logs.

### Low
6. **Security documentation still describes demo fallback path**
   - Location: `docs/security.md`
   - Risk: Teams may normalize insecure fallback patterns.
   - Recommendation: Mark demo paths as temporary and add removal deadline.

## Immediate action list

1. Remove hardcoded fallback credentials.
2. Replace string-based auth outcomes with typed enum.
3. Force debug-only auth via compile-time option.
4. Add auth attempt throttling and audit events.
5. Update docs to production-ready flow only.


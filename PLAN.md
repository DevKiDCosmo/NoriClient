# PLAN for me

- Adding console that just the logger.
- Adding global .env variable handler.
- Adding the full new logger `debug` and `fallback`.
- Updating Main.cpp and move things to uri etc.
- Updating reason for authentication. Or make a function where the process is called and it is just going through.
- Adding new msg for id login.
- Connecting loose function to any header

# Loose Function
Freie Top-Level-Funktionen
src/main.cpp — main
src/network/uri/uriHandler.cpp — routingHandler
src/network/uri/util/validate.cpp — toASCII_IDN
src/network/uri/util/validate.cpp — isAllDigits
Platzhalter/Stubs
src/biometric/BiometricAuth_stub.cpp — Stub-Implementierung, leere Bodies / AuthResult::Failed
src/defines/runtime/runtimeGlobal.h — leere Klasse runtimeGlobal
src/defines/runtime/runtimeGlobal.cpp — nur #include, keine Logik

Lose Funktionen ohne Header-/Klassenbindung
src/network/uri/uriHandler.cpp — routingHandler(...)
src/network/uri/util/validate.cpp — toASCII_IDN(...)
src/network/uri/util/validate.cpp — isAllDigits(...)
Dateien, die nach späteren Platzhaltern / Auslagerungskandidaten aussehen
src/biometric/auth-id/IDFallback.cpp
src/biometric/auth-id/IDFallback.h
src/defines/runtime/runtimeGlobal.cpp
src/defines/runtime/runtimeGlobal.h
src/biometric/BiometricAuth_stub.cpp

# Add different URI Processes

- `nori-auth://biometric` - for biometric authentication flow.
- `nori-api://` - Handles all app and api related stuff.
   - Api can also create socket creation for client heartbeat
   - Installation Approval
   - Version and installation.
   - Configuration
   - Force Update.
   - automatiticity

- `nori-slk://` - Single Login Key.
- `nori-request://` - for login related actions, with subtypes for different login methods (e.g., `nori-login://password`, `nori-login://native`).

# Update
Client download zip and creates script.
Client runs scripts and kill itself
Script check every second if client is correctly killed.
Script remvoe old client and install new client. Extract...
Script Start new client
Client notice a script start because of command line `APPLICATION -fresh-install "{localstion of script}"`
Client remove script.
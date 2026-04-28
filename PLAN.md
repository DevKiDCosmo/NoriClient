# PLAN for me

- Adding console that just the logger.
- Adding global .env variable handler.
- Adding the full new logger `debug` and `fallback`.
- Updating Main.cpp and move things to uri etc.
- Updating reason for authentication. Or make a function where the process is called and it is just going through.
- Adding new msg for id login.
- Connecting loose function to any header
- If multiple authentication required. Create list what should be autenticate. First normal flow then a list with all action that can be run. This ensure a one login instead multiple.

- Adding PORT to chain. Instead only 

```cpp
network::request::ProtocolChain chain = network::request::ProtocolChain::create({
  "kaizo://", "kzps://", "nori-slk://", "nori-loop://", "nori-auth://", "https://", "http://"
});
```

```cpp
network::request::ProtocolChain chain = network::request::ProtocolChain::create({
  {"kaizo://", 0}, {"kzps://", 0}, {"nori-slk://", 0}, {"nori-loop://", 0}, {"nori-auth://", 0}, {"https://", 443}, {"http://", 80}
});
```

- Platform compactble for other and also in action add to cmake also release and do not use debug
- Update Info-plist so there is an icon at laest.

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
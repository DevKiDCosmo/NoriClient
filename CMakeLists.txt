cmake_minimum_required(VERSION 4.2)
project(NoriID LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
		nlohmann_json
		GIT_REPOSITORY https://github.com/nlohmann/json.git
		GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ---- STRANGE
find_package(PkgConfig REQUIRED)
pkg_check_modules(IDN2 REQUIRED libidn2)

add_library(idn2::idn2 UNKNOWN IMPORTED)
set_target_properties(idn2::idn2 PROPERTIES
		IMPORTED_LOCATION "${IDN2_LINK_LIBRARIES}"
		INTERFACE_INCLUDE_DIRECTORIES "${IDN2_INCLUDE_DIRS}"
)
# ---- STRANGE

find_package(CURL REQUIRED)

add_executable(NoriID MACOSX_BUNDLE src/main.cpp
		src/logs/logger.cpp
		src/logs/logger.h
		src/biometric/auth-id/IDFallback.cpp
		src/biometric/auth-id/IDFallback.h
		src/defines/defines.h
		src/network/request/miniRequestX.cpp
		src/network/request/miniRequestX.h
		src/defines/env/env.cpp
		src/defines/env/env.h
		src/defines/runtime/runtimeGlobal.cpp
		src/defines/runtime/runtimeGlobal.h
		src/network/uri/uriHandler.cpp
		src/network/uri/uriHandler.h
		src/routine/init.cpp
		src/routine/init.h
		src/network/uri/util/validate.cpp
		src/network/uri/util/utilURI.h
		src/network/uri/routes/slk.cpp
		src/network/uri/routes/api.cpp
		src/network/uri/routes/auth.cpp
		src/network/uri/routes/request.cpp
		src/network/uri/routes/routes.h
		src/network/uri/util/micsURI.cpp)

set_target_properties(NoriID PROPERTIES
	MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
)

if(APPLE)
	target_sources(NoriID PRIVATE src/biometric/BiometricAuth.mm)
	target_sources(NoriID PRIVATE src/ui/AppController.mm)
	target_sources(NoriID PRIVATE src/ui/Reponses/Dialogs.mm)
	target_link_libraries(NoriID PRIVATE "-framework AppKit" "-framework LocalAuthentication" "-framework Foundation" "-framework Security")
else()
	target_sources(NoriID PRIVATE src/biometric/BiometricAuth_stub.cpp)
endif()

target_link_libraries(NoriID PRIVATE CURL::libcurl nlohmann_json::nlohmann_json idn2::idn2)

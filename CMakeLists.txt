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

find_package(CURL REQUIRED)

add_executable(NoriID src/main.cpp
		src/logs/logger.cpp
		src/logs/logger.h)

if(APPLE)
	target_sources(NoriID PRIVATE src/biometric/BiometricAuth.mm)
	target_link_libraries(NoriID PRIVATE "-framework LocalAuthentication" "-framework Foundation")
else()
	target_sources(NoriID PRIVATE src/biometric/BiometricAuth_stub.cpp)
endif()

target_link_libraries(NoriID PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

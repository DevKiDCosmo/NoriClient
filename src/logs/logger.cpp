#include "logger.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace {
std::mutex g_loggerMutex;
std::atomic_bool g_colorsEnabled{true};

std::string timestampNow() {
	const auto now = std::chrono::system_clock::now();
	const std::time_t raw = std::chrono::system_clock::to_time_t(now);

	std::tm localTime{};
#if defined(_WIN32)
	localtime_s(&localTime, &raw);
#else
	localtime_r(&raw, &localTime);
#endif

	std::ostringstream output;
	output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
	return output.str();
}
} // namespace

void logger::setColorsEnabled(const bool enabled) {
	g_colorsEnabled.store(enabled);
}

void logger::log(const LogType type, const std::string_view message) {
	std::lock_guard<std::mutex> lock(g_loggerMutex);

	std::ostream &stream = (type == LogType::Warning || type == LogType::Error || type == LogType::Fatal)
							   ? std::cerr
							   : std::cout;

	const std::string stamp = timestampNow();
	if (g_colorsEnabled.load()) {
		stream << color(type) << '[' << stamp << "] [" << label(type) << "] " << message << "\033[0m\n";
	} else {
		stream << '[' << stamp << "] [" << label(type) << "] " << message << '\n';
	}
}

void logger::information(const std::string_view message) {
	log(LogType::Information, message);
}

void logger::system(const std::string_view message) {
	log(LogType::System, message);
}

void logger::response(const std::string_view message) {
	log(LogType::Response, message);
}

void logger::api(const std::string_view message) {
	log(LogType::Api, message);
}

void logger::socket(const std::string_view message) {
	log(LogType::Socket, message);
}

void logger::warning(const std::string_view message) {
	log(LogType::Warning, message);
}

void logger::error(const std::string_view message) {
	log(LogType::Error, message);
}

void logger::fatal(const std::string_view message) {
	log(LogType::Fatal, message);
}

void logger::hint(const std::string_view message) {
	log(LogType::Hint, message);
}

void logger::important(const std::string_view message) {
	log(LogType::Important, message);
}

void logger::init(const std::string_view message) {
	log(LogType::Init, message);
}

void logger::notImplemented(const std::string_view message) {
	log(LogType::notImplemented, message);
}

const char *logger::label(const LogType type) {
	switch (type) {
		case LogType::Information:
			return "INFORMATION";
		case LogType::System:
			return "SYSTEM";
		case LogType::Response:
			return "RESPONSE";
		case LogType::Api:
			return "API";
		case LogType::Socket:
			return "SOCKET";
		case LogType::Warning:
			return "WARNING";
		case LogType::Error:
			return "ERROR";
		case LogType::Fatal:
			return "FATAL";
		case LogType::Hint:
			return "HINT";
		case LogType::Important:
			return "IMPORTANT";
		case LogType::Init:
			return "INIT";
		case LogType::notImplemented:
			return "NOT IMPLEMENTED";
	}

	return "UNKNOWN";
}

const char *logger::color(const LogType type) {
	switch (type) {
		case LogType::Information:
			return "\033[37m";
		case LogType::System:
			return "\033[36m";
		case LogType::Response:
			return "\033[32m";
		case LogType::Api:
			return "\033[34m";
		case LogType::Socket:
			return "\033[35m";
		case LogType::Warning:
			return "\033[33m";
		case LogType::Error:
			return "\033[31m";
		case LogType::Fatal:
			return "\033[1;31m";
		case LogType::Hint:
			return "\033[96m";
		case LogType::Important:
			return "\033[1;93m";
		case LogType::Init:
			return "\033[1;95m";
		case LogType::notImplemented:
			return "\033[1;91m";
	}

	return "\033[37m";
}

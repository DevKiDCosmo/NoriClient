#pragma once

#include <string_view>

enum class LogType {
	Information,
	System,
	Response,
	Api,
	Socket,
	Warning,
	Error,
	Fatal,
	Hint,
	Important,
	Init
};

class logger {
public:
	static void setColorsEnabled(bool enabled);
	static void log(LogType type, std::string_view message);

	static void information(std::string_view message);
	static void system(std::string_view message);
	static void response(std::string_view message);
	static void api(std::string_view message);
	static void socket(std::string_view message);
	static void warning(std::string_view message);
	static void error(std::string_view message);
	static void fatal(std::string_view message);
	static void hint(std::string_view message);
	static void important(std::string_view message);
	static void init(std::string_view message);

	static void fallback(std::string_view message);
	static void debug(std::string_view message);

private:
	static const char *label(LogType type);
	static const char *color(LogType type);
};
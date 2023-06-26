#pragma once
#include <string>
#include <Windows.h>

#define FILE_NAME L"debug_dump.txt"
#define STENCIL GetTimestamp() + "(at line: " + std::to_string(line) + " in file: " + file + ") "

namespace logger
{
	void Log(const std::string& text, const std::string& file, unsigned int line);

	void LogError_(const std::string& text, const std::string& file, unsigned int line);

	void LogWarning_(const std::string& text, const std::string& file, unsigned int line);

	void LogInfo_(const std::string& text, const std::string& file,  unsigned int line);

#define LogError(x) LogError_(x, __FILE__, __LINE__)
#define LogWarning(x) LogWarning_(x, __FILE__, __LINE__)
#define LogInfo(x) LogInfo_(x, __FILE__, __LINE__)
}
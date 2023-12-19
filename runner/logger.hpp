#pragma once
#include <string>
#include <Windows.h>

#define FILE_NAME L"debug_dump.txt"
#define STENCIL GetTimestamp() + "(at line: " + std::to_string(line) + " in file: " + file + ") "
#define LogError(message) logger::LogError_(message, __FILE__, __LINE__)
#define LogWarning(message) logger::LogWarning_(message, __FILE__, __LINE__)
#define LogInfo(message) logger::LogInfo_(message, __FILE__, __LINE__)

namespace logger
{
	void Log(const std::string& text, const std::string& file, unsigned int line);

	void LogError_(const std::string& text, const std::string& file, unsigned int line);

	void LogWarning_(const std::string& text, const std::string& file, unsigned int line);

	void LogInfo_(const std::string& text, const std::string& file,  unsigned int line);
}
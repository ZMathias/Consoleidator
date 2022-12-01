#pragma once
#include <string>
#include <Windows.h>

#define FILE_NAME L"debug_dump.txt"
#define STENCIL GetTimestamp() + "(at line: " + std::to_string(line) + " in file: " + file + ") "

namespace logger
{
	void Log(const std::string& text, const std::string& file, unsigned int line);

	void LogError(const std::string& text, const std::string& file, unsigned int line);

	void LogWarning(const std::string& text, const std::string& file, unsigned int line);

	void LogInfo(const std::string& text, const std::string& file,  unsigned int line);
}
#pragma once
#include <string>
#include <Windows.h>

#define FILE_NAME L"debug_dump.txt"

namespace logger
{
	void Log(const std::string& text, const std::string& file, unsigned int line);

	void LogError(const std::string& text, const std::string& file, unsigned int line);

	void LogWarning(const std::string& text, const std::string& file, unsigned int line);

	void LogInfo(const std::string& text, const std::string& file,  unsigned int line);
}
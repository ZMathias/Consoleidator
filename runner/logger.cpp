#pragma once
#include "logger.hpp"

// writes anything to the debug file
void logger::Log(const std::string& text, const std::string& file, const unsigned int line)
{
	HANDLE hFile{};
	hFile = CreateFile(FILE_NAME, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		MessageBoxW(nullptr, L"failed to open debug dump\nreason: invalid handle value returned from CreateFileW", L"Error", MB_OK);
		return;
	}
	WriteFile(hFile, text.data(), static_cast<DWORD>(text.size()), nullptr, nullptr);
	CloseHandle(hFile);
}

void logger::LogError(const std::string& text, const std::string& file, const unsigned int line)
{
	const std::string trace = "[ERROR] (at line: " + std::to_string(line) + " in file: " + file + ") " + text + "\n";
	Log(trace, file, line);
}

void logger::LogWarning(const std::string& text, const std::string& file, const unsigned int line)
{
	const std::string trace = "[WARN](at line:" + std::to_string(line) + " in file: " + file + ") " + text;
	Log(trace, file, line);
}

void logger::LogInfo(const std::string& text, const std::string& file, const unsigned int line)
{
	const std::string trace = "[INFO](at line:" + std::to_string(line) + " in file: " + file + ") " + text;
	Log(trace, file, line);	
}
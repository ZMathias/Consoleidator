// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "logger.hpp"

// writes anything to the debug file
void logger::Log(const std::string& text, const std::string& file, const unsigned int line)
{
	HANDLE hFile{};
	hFile = CreateFile(FILE_NAME, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		MessageBoxW(nullptr, (L"failed to open debug dump\nreason: invalid handle with error: " + std::to_wstring(GetLastError())).c_str(), L"Consoleidator error dialog", MB_OK);
		return;
	}
	WriteFile(hFile, text.data(), static_cast<DWORD>(text.size()), nullptr, nullptr);
	CloseHandle(hFile);
}

std::string GetTimestamp()
{
	SYSTEMTIME time{};
	GetLocalTime(&time);
	std::string timest;
	timest += "[";
	timest += std::to_string(time.wYear);
	timest += '/';
	timest += std::to_string(time.wMonth);
	timest += '/';
	timest += std::to_string(time.wDay);
	timest += ' ';
	timest += std::to_string(time.wHour);
	timest += ':';
	timest += std::to_string(time.wMinute);
	timest += ':';
	timest += std::to_string(time.wSecond);
	timest += ']';
	return timest;
}

void logger::LogError_(const std::string& text, const std::string& file, const unsigned int line)
{
	const std::string trace = "[ERROR]" + STENCIL + text + "\r\n";
	Log(trace, file, line);
}

void logger::LogWarning_(const std::string& text, const std::string& file, const unsigned int line)
{
	const std::string trace = "[WARN]" + STENCIL + text + "\r\n";
	Log(trace, file, line);
}

void logger::LogInfo_(const std::string& text, const std::string& file, const unsigned int line)
{
	const std::string trace = "[INFO]" + STENCIL + text + "\r\n";
	Log(trace, file, line);	
}
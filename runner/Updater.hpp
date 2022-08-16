// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include <codecvt>
#include <fstream>
#include <locale>
#include <Windows.h>
#include <WinInet.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "runner-constants.hpp"

class Updater
{
	const std::string VersionUrl{"https://api.github.com/repos/ZMathias/Consoleidator/releases/latest"};
	int LocalVersion{};


	[[nodiscard]] std::vector<char> MakeRequest(const std::string& url) const;
	[[nodiscard]] std::wstring DownloadFile(const std::string& url, const std::wstring& fileName) const;
	[[nodiscard]] nlohmann::json CheckForUpdates() const;
	[[nodiscard]] int ParseToVersion(const std::string& str) const;
	[[nodiscard]] std::wstring RenameOld(const wchar_t* lpPath) const;
	[[nodiscard]] std::vector<std::wstring> ScanForCorrespondingFiles(const wchar_t*) const;
	[[nodiscard]] std::wstring GetImageDirectory();
	[[nodiscard]] std::vector<std::wstring> DownloadAndDumpFiles(const nlohmann::json& json) const;

	static std::wstring to_utf8(const std::string& string)
	{
		std::wstring str;
		str.reserve(string.size());
	    for (const auto& c : string)
			str += c;
		return str;
	}

public:
	std::wstring ImageDirectory{};
	explicit Updater(const std::string&& image_version);
	int GetMajorWindowsVersion() const;
};


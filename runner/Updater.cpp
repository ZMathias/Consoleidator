// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Updater.hpp"

int Updater::GetMajorWindowsVersion() const
{
	DWORD size{ MAX_PATH };
	char* buff = new char[size] {};
	LSTATUS errorCode{};
	errorCode = RegGetValueA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber", RRF_RT_REG_SZ | RRF_RT_REG_QWORD | RRF_RT_REG_DWORD, nullptr, buff, &size);
	if (errorCode != ERROR_SUCCESS)
		return -1;
	return std::atoi(buff);
}

std::vector<char> Updater::MakeRequest(const std::string& url) const
{
	constexpr DWORD buffer_size = 100000;
	HINTERNET hInternet = InternetOpenA(
		"Consoleidator",
		INTERNET_OPEN_TYPE_DIRECT, 
		nullptr, 
		nullptr,
		0
	);

	if (hInternet == nullptr)
	{
		DWORD errorSrc[MAX_PATH]{};
		char errorMsg[MAX_PATH]{};
		DWORD len{MAX_PATH};
		InternetGetLastResponseInfoA(errorSrc, errorMsg, &len);
		throw std::exception(("InternetOpenA failed with: " + std::to_string(GetLastError()) + " and " + errorMsg + "\n").c_str());
	}
		

	HINTERNET hFile = InternetOpenUrlA(
		hInternet, 
		url.c_str(), 
		nullptr, 
		0, 
		INTERNET_FLAG_RELOAD, 
		NULL
	);

	if (hFile == nullptr)
	{
		throw std::exception(("InternetOpenUrlA failed with: " + std::to_string(GetLastError()) + '\n').c_str());
	}
	
	auto data = new char[buffer_size]{};
	DWORD read{};
	std::vector<char> buffer;

	do
	{
		if (InternetReadFile(hFile, data, buffer_size, &read))
		{
			buffer.insert(buffer.end(), data, data + read);
		}
		else
		{
			DWORD errorSrc[MAX_PATH]{};
			char errorMsg[MAX_PATH]{};
			DWORD len{MAX_PATH};
			InternetGetLastResponseInfoA(errorSrc, errorMsg, &len);
			throw std::exception(("InternetReadFile failed with: " + std::to_string(GetLastError()) + " and " + errorMsg).c_str());
		}
	}
	while (read != 0);
	delete[] data;

	return buffer;
}

nlohmann::json Updater::CheckForUpdates() const
{
	const auto buffer = MakeRequest(VersionUrl);
	if (buffer.empty()) return false;
	nlohmann::json json = nlohmann::json::parse(buffer);

	std::string versionStr;
	json["tag_name"].get_to(versionStr);

	if (ParseToVersion(versionStr) <= this->LocalVersion) return {};
	return json;
}

std::wstring Updater::DownloadFile(const std::string& url, const std::wstring& fileName) const
{
	std::wstring fullPath = ImageDirectory + fileName;
	const auto buffer = MakeRequest(url);
	if (buffer.empty()) return {};
	std::ofstream file(fullPath, std::ios::binary);
	file.write(buffer.data(), buffer.size());
	file.close();
	return fullPath;
}

std::vector<std::wstring> Updater::DownloadAndDumpFiles(const nlohmann::json& json) const
{
	std::vector<std::wstring> files;
	const auto assetSize = json["assets"].size();
	files.reserve(assetSize - 1);
	for (size_t i = 0; i < assetSize; ++i)
	{
		std::string assetName;
		json["assets"][i]["name"].get_to(assetName);
		if (assetName.find(".zip") == std::string::npos)
		{
			std::string url, fileName;
			json["assets"][i]["browser_download_url"].get_to(url);
			json["assets"][i]["name"].get_to(fileName);
			files.emplace_back(DownloadFile(url, to_utf8(fileName)));
		}
	}
	return files;
}

int Updater::ParseToVersion(const std::string& str) const
{
	int version{};
	for (const auto& t : str)
		if (std::isdigit(t))
			version = version * 10 + t - '0';			
	return version;
}

std::wstring Updater::GetImageDirectory()
{
	// make a call to QueryFullProcessImageNameW
	// to get the image name of the process
	DWORD dwSize = MAX_PATH * 2;
	const std::unique_ptr<wchar_t[]> lpImageName = std::make_unique<wchar_t[]>(dwSize);
	if (QueryFullProcessImageNameW(GetCurrentProcess(), 0, lpImageName.get(), &dwSize))
	{
		// get the directory of the image name
		const std::wstring ws(lpImageName.get());
		const std::size_t found = ws.find_last_of(L'\\');
		return ws.substr(0, found + 1);
	}
	MessageBoxA(nullptr, ("error querying image path " + std::to_string(GetLastError())).data(), "Error", MB_ICONERROR);
	return {};
}

std::vector<std::wstring> Updater::ScanForCorrespondingFiles(const wchar_t* token) const
{
	std::vector<std::wstring> files;
	WIN32_FIND_DATA find_data{};

	const auto findStr = this->ImageDirectory + token;
	const auto hFile = FindFirstFileW(findStr.data(), &find_data);

	if (hFile == INVALID_HANDLE_VALUE) return {};

	files.emplace_back(ImageDirectory + find_data.cFileName);
	while (FindNextFileW(hFile, &find_data))
		files.emplace_back(ImageDirectory + find_data.cFileName);
	return files;
}

std::wstring Updater::RenameOld(const wchar_t* lpPath) const
{
	if (*lpPath == L'\0') return {};
	const std::unique_ptr<wchar_t[]> lpNewPath = std::make_unique<wchar_t[]>(MAX_PATH * 2);
	const std::unique_ptr<wchar_t[]> lpExtension = std::make_unique<wchar_t[]>(10);
	const std::wstring old{L"_OLD"};

	const auto len = wcslen(lpPath);
	const auto extOffset = wcsrchr(lpPath, L'.') - lpPath;
	const auto extLen = len - extOffset;

	// get extension
	wmemcpy(lpExtension.get(), lpPath + extOffset, extLen);

	// copy until extension
	wmemcpy(lpNewPath.get(), lpPath, extOffset);

	// append _OLD to the end
	wmemcpy(lpNewPath.get() + extOffset, old.data(), old.size());

	// add back the file extension
	wmemcpy(lpNewPath.get() + extOffset + extLen, lpExtension.get(), extLen);

	if (!MoveFileW(lpPath, lpNewPath.get())) return {};
	return {lpNewPath.get()};
}

Updater::Updater(const std::string&& image_version)
{
	LocalVersion = ParseToVersion(image_version);
	ImageDirectory = GetImageDirectory();

	const auto residueFiles = ScanForCorrespondingFiles(L"*_OLD*");

	for (const auto& file : residueFiles)
		if (DeleteFileW(file.data()) == 0)
			MessageBoxA(nullptr, ("error deleting file " + std::to_string(GetLastError())).data(), "Error", MB_ICONERROR);

	if (!residueFiles.empty())
		return;

	// if no updates are available, we return an empty json object
	const auto json = CheckForUpdates();

	// if it's empty, we're done
	if (json.empty()) return;

	const auto files = ScanForCorrespondingFiles(L"consoleidator*");
	std::vector<std::wstring> oldFiles;
	oldFiles.reserve(files.size());

	for (const auto& file : files)
		oldFiles.emplace_back(RenameOld(file.data()));

	const auto newFiles = DownloadAndDumpFiles(json);
	if (files.empty()) return;

	const wchar_t* runFile{};
	// find consoleidator.exe
	for (const auto& file : newFiles)
		if (file.find(L"consoleidator.exe") != std::wstring::npos)
		{
			runFile = file.data();
			break;
		}

	// set up call to CreateProcessW
	STARTUPINFO si{};
	PROCESS_INFORMATION pi{};
	SecureZeroMemory(&si, sizeof(si));
	SecureZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof si;
	BOOL success = CreateProcessW(
		runFile,
		nullptr,
		nullptr,
		nullptr,
		FALSE,
		NORMAL_PRIORITY_CLASS | CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
		nullptr,
		ImageDirectory.data(),
		&si,
		&pi
		);

	if (!success)
	{
		const std::string error = "Failed to open process";
		MessageBoxA(nullptr, error.c_str(), "Error", MB_ICONERROR);
		logger::LogError(error, __FILE__, __LINE__);
	}

	CloseHandle(pi.hProcess);
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	exit(0);
}
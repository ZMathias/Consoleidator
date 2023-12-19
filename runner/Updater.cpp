// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Updater.hpp"

int Updater::GetMajorWindowsVersion()
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

void Updater::DownloadFile(const std::string& url, const std::wstring& fileName) const
{
	const std::wstring fullPath = ExecutableDirectory + fileName;
	const auto buffer = MakeRequest(url);
	if (buffer.empty()) return;
	std::ofstream file(fullPath, std::ios::binary);
	file.write(buffer.data(), buffer.size());
	file.close();
}

void Updater::DownloadAndDumpFiles(const nlohmann::json& json) const
{
	const auto assetSize = json["assets"].size();
	for (size_t i = 0; i < assetSize; ++i)
	{
		std::string assetName;
		json["assets"][i]["name"].get_to(assetName);
		// if not a zip file, then download it
		if (assetName.find(".zip") == std::string::npos)
		{
			std::string url, fileName;
			json["assets"][i]["browser_download_url"].get_to(url);
			json["assets"][i]["name"].get_to(fileName);
			DownloadFile(url, to_wchar(fileName));
		}
	}
}

int Updater::ParseToVersion(const std::string& str) const
{
	int version{};
	for (const auto& t : str)
		if (std::isdigit(t))
			version = version * 10 + t - '0';			
	return version;
}

std::wstring Updater::GetExecutableDirectory()
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

	const auto findStr = this->ExecutableDirectory + token;
	const auto hFile = FindFirstFileW(findStr.data(), &find_data);

	if (hFile == INVALID_HANDLE_VALUE) return {};

	files.emplace_back(ExecutableDirectory + find_data.cFileName);
	while (FindNextFileW(hFile, &find_data))
		files.emplace_back(ExecutableDirectory + find_data.cFileName);
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
	ExecutableDirectory = GetExecutableDirectory();

	// CHECK PRIVILEGES
#ifndef _DEBUG
	if (!HasAdminPrivileges())
	{
		//LogWarning("The program was started without administrative privileges, restarting with UAC.");
		RestartElevated();
	}
#endif

	// UPDATE ROUTINE
	// check for files left after update
	const auto residueFiles = ScanForCorrespondingFiles(L"*_OLD*");

	size_t deleteCounter{};
	for (const auto& file : residueFiles)
	{
		if (DeleteFileW(file.data()) == 0)
		{
			LogError("error deleting file " + std::to_string(GetLastError()));
			Restart();
		}
		else
		{
			deleteCounter++;
		}
	}

	if (deleteCounter != 0 && deleteCounter == residueFiles.size())
	{
		LogInfo("successfully deleted " + std::to_string(deleteCounter) + " old files.");
		LogInfo("Update complete.");
	}

	if (!residueFiles.empty())
		return;

#ifdef _STRIPPED_NO_UPDATE_
	return;
#endif
#ifndef _STRIPPED_NO_UPDATE_
	// if no updates are available, we return an empty json object
	const auto json = CheckForUpdates();

	// if it's empty, we're done
	if (json.empty()) 
	{
		LogInfo("No updates available. Current version: " + image_version);
		return;
	}

	LogInfo("update available: " + json["tag_name"].get<std::string>() + ", current version: " + image_version + ". Proceeding with update.");

	const auto files = ScanForCorrespondingFiles(L"consoleidator*");
	std::vector<std::wstring> oldFiles;
	oldFiles.reserve(files.size());

	for (const auto& file : files)
		oldFiles.emplace_back(RenameOld(file.data()));

	DownloadAndDumpFiles(json);
	if (files.empty()) return;
	// UPDATE ROUTINE OVER

	Restart();
#endif
}

bool Updater::HasAdminPrivileges()
{
	BOOL fIsRunAsAdmin = FALSE;
    DWORD dwError = ERROR_SUCCESS;
    PSID pAdministratorsGroup = nullptr;

    // Allocate and initialize a SID of the administrators group.
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(
        &NtAuthority, 
        2, 
        SECURITY_BUILTIN_DOMAIN_RID, 
        DOMAIN_ALIAS_RID_ADMINS, 
        0, 0, 0, 0, 0, 0, 
        &pAdministratorsGroup))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Determine whether the SID of administrators group is enabled in 
    // the primary access token of the process.
    if (!CheckTokenMembership(nullptr, pAdministratorsGroup, &fIsRunAsAdmin))
    {
        dwError = GetLastError();
    }

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (pAdministratorsGroup)
    {
        FreeSid(pAdministratorsGroup);
        pAdministratorsGroup = nullptr;
    }

    // Throw the error if something failed in the function.
    if (ERROR_SUCCESS != dwError)
    {
		LogError("Couldn't check rights for administrative privileges");
        return false;
    }

    return fIsRunAsAdmin;
}

// this restarts the process as is all the while retaining the original privileges
void Updater::Restart()
{
	LogInfo("Restart requested.");
	const std::wstring ExecutableDirectory = GetExecutableDirectory();
	const std::wstring runFile = ExecutableDirectory + L"consoleidator.exe";

	// set up call to CreateProcessW
	STARTUPINFO si{};
	PROCESS_INFORMATION pi{};
	SecureZeroMemory(&si, sizeof(si));
	SecureZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof si;
	const BOOL success = CreateProcessW(
		runFile.c_str(),
		nullptr,
		nullptr,
		nullptr,
		FALSE,
		NORMAL_PRIORITY_CLASS | CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
		nullptr,
		ExecutableDirectory.data(),
		&si,
		&pi
		);

	if (!success)
	{
		const std::string error = "Failed to open process, aborting restart.";
		MessageBoxA(nullptr, error.c_str(), "Restart aborted", MB_ICONERROR);
		LogError(error);
		return;
	}

	CloseHandle(pi.hProcess);
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	exit(0);
}

void Updater::RestartElevated()
{
	//LogInfo("Elevated restart requested.");
	const std::wstring ExecutablePath = GetExecutableDirectory();
	const std::wstring runFile = ExecutablePath + L"consoleidator.exe";

	HINSTANCE hInst = ShellExecuteW(
		nullptr, 
		L"runas", 
		runFile.c_str(), 
		nullptr, 
		ExecutablePath.c_str(), 
		TRUE
	);

	if ((INT_PTR)hInst <= 32)
	{
		const std::string error = "Failed to restart with elevated privileges. Aborting...";
		MessageBoxA(nullptr, error.c_str(), "Restart aborted", MB_ICONERROR);
		LogError(error);
		return;
	}

	//LogInfo("Successfully spawned new process with elevated privileges. Exiting...");

	exit(0);
}
// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com


#include "startup.hpp"

#define PREDEF_KEY HKEY_LOCAL_MACHINE
#define REGISTRY_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define REGISTRY_VALUE L"Consoleidator"

BOOL AddToStartup()
{
	WCHAR wPathToImage[500]{};
	GetModuleFileNameW(nullptr, wPathToImage, 500);

	HKEY hKey{};
	LRESULT lResult =
		RegOpenKeyExW(
			PREDEF_KEY,
			REGISTRY_KEY,
			NULL,
			KEY_ALL_ACCESS,
			&hKey
		);

	if (lResult != ERROR_SUCCESS)
	{
		if (lResult == ERROR_FILE_NOT_FOUND)
		{
			MessageBoxW(
				nullptr, L"Unable to add to write startup registry.\nCheck if the program has enough privileges.",
				L"Error", MB_ICONERROR);
			LogError("Registry key not found while opening startup registry key");
			return TRUE;
		}
		MessageBoxW(nullptr, L"Unable to add to write startup registry.\nCheck if the program has enough privileges.",
		            L"Error", MB_ICONERROR);
		LogError("Access denied writing to registry.");
		return FALSE;
	}

	lResult = RegSetValueExW(
		hKey,
		REGISTRY_VALUE,
		NULL, REG_SZ,
		(LPBYTE)wPathToImage,
		sizeof wPathToImage
	);

	if (lResult != ERROR_SUCCESS)
	{
		MessageBoxW(nullptr, L"Unable to add to write startup registry.\nCheck if the program has enough privileges.",
		            L"Error", MB_ICONERROR);
		LogError("Access denied writing to registry.");
		return FALSE;
	}

	CloseHandle(hKey);
	return TRUE;
}

BOOL RemoveFromStartup()
{
	WCHAR wPathToImage[500]{};
	GetModuleFileNameW(nullptr, wPathToImage, 500);

	HKEY hKey{};
	LRESULT lResult =
		RegOpenKeyExW(
			PREDEF_KEY,
			REGISTRY_KEY,
			NULL,
			KEY_ALL_ACCESS,
			&hKey
		);

	if (lResult != ERROR_SUCCESS)
	{
		if (lResult == ERROR_FILE_NOT_FOUND)
		{
			MessageBoxW(
				nullptr, L"Unable to delete from startup registry.\nCheck if the program has sufficient privileges.",
				L"Error", MB_ICONERROR);
			LogError("Registry key not found while opening startup registry key");
			CloseHandle(hKey);
			return TRUE;
		}
		MessageBoxW(nullptr, L"Unable to delete from startup registry.\nCheck if the program has sufficient privileges.",
		            L"Error", MB_ICONERROR);
		LogError("Access denied writing to registry.");
		CloseHandle(hKey);
		return FALSE;
	}

	lResult = RegDeleteValueW(hKey, REGISTRY_VALUE);

	if (lResult != ERROR_SUCCESS)
	{
		if (lResult == ERROR_FILE_NOT_FOUND)
		{
			MessageBoxW(
				nullptr,
				L"Unable to delete from startup registry.\nIt probably wasn't present.\nCheck the log for more information.",
				L"Error", MB_ICONERROR);
			LogError("Unable to delete value \"Consoleidator\" due to reason: ERROR_FILE_NOT_FOUND");
			CloseHandle(hKey);
			return FALSE;
		}
		MessageBoxW(nullptr, L"Unable to delete from startup registry.\nCheck if the program has sufficient privileges.",
		            L"Error", MB_ICONERROR);
		LogError("Access denied writing to registry.");
		CloseHandle(hKey);
		return FALSE;
	}

	CloseHandle(hKey);
	return TRUE;
}

// disables the "Run at startup" menu item
void SetStartupCheckboxGreyed(HWND hWnd, bool state)
{
	// toggle menu item
	HMENU hMenu{};
	hMenu = GetMenu(GetActiveWindow());

	MENUITEMINFO menuInfo{};
	menuInfo.cbSize = sizeof MENUITEMINFO;
	menuInfo.fMask = MIIM_STATE;
	GetMenuItemInfo(hMenu, 0, FALSE, &menuInfo);
	menuInfo.fState = state ? MFS_ENABLED : MFS_DISABLED;
	SetMenuItemInfo(hMenu, 0, FALSE, &menuInfo);
}

// toggles the checkmark state depending on the current state
void ToggleStartup(HWND hWnd)
{
	HMENU hMenu = GetMenu(hWnd);

	MENUITEMINFO menuItem{};
	menuItem.cbSize = sizeof MENUITEMINFO;
	menuItem.fMask = MIIM_STATE;

	GetMenuItemInfo(hMenu, ID_OPTIONS_RUNATSTARTUP, FALSE, &menuItem);

	if (menuItem.fState == MFS_CHECKED)
	{
		// Checked, uncheck it
		menuItem.fState = RemoveFromStartup() ? MFS_UNCHECKED : MFS_CHECKED;
	}
	else
	{
		// Unchecked, check it
		menuItem.fState = AddToStartup() ? MFS_CHECKED : MFS_UNCHECKED;
	}

	SetMenuItemInfo(hMenu, ID_OPTIONS_RUNATSTARTUP, FALSE, &menuItem);
}

// sets the checkmark in the menu according to the state parameter
void SetStartupCheckmark(HWND hWnd, bool state)
{
	HMENU hMenu = GetMenu(hWnd);

	MENUITEMINFO menuItem{};
	menuItem.cbSize = sizeof MENUITEMINFO;
	menuItem.fMask = MIIM_STATE;

	GetMenuItemInfo(hMenu, ID_OPTIONS_RUNATSTARTUP, FALSE, &menuItem);

	menuItem.fState = state ? MFS_CHECKED : MFS_UNCHECKED;

	SetMenuItemInfo(hMenu, ID_OPTIONS_RUNATSTARTUP, FALSE, &menuItem);
}

// queries if the image is set to start with the system
int IsStartup()
{
	HKEY hKey{};
	LRESULT lResult =
		RegOpenKeyExW(
			PREDEF_KEY,
			REGISTRY_KEY,
			NULL,
			KEY_ALL_ACCESS,
			&hKey
		);

	if (lResult != ERROR_SUCCESS)
	{
		// couldn't open registry path
		if (lResult == ERROR_FILE_NOT_FOUND)
		{
			LogError("Registry key not found while opening startup registry key,");
			CloseHandle(hKey);
			return -1;
		}

		// disable the startup checkbox
		LogError("Access denied writing to registry. GetLastError returned: " + std::to_string(GetLastError()));
		CloseHandle(hKey);
		// TODO: fix user alerts
		// the caller should dispatch alerts based on return value
		return -1;
	}

	lResult = RegQueryValueExW(
		hKey,
		REGISTRY_VALUE,
		nullptr,
		nullptr,
		nullptr,
		nullptr
	);

	// this is business logic, not error handling
	if (lResult != ERROR_SUCCESS)
	{
		if (lResult == ERROR_FILE_NOT_FOUND)
		{
			// the value is not present, thus the program is not set to start with the system
			CloseHandle(hKey);
			return FALSE;
		}
		LogError("An error occured while checking for startup presence in the registry.");
		CloseHandle(hKey);
		return FALSE;
	}

	// the value is present, thus the program is set to start with the system
	CloseHandle(hKey);
	return TRUE;
}

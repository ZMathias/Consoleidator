#include "startup.hpp"

BOOL AddToStartup()
{
	WCHAR wPathToImage[500]{};
	GetModuleFileNameW(nullptr, wPathToImage, 500);

	HKEY hKey{};
	LRESULT lResult = 
		RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", NULL, KEY_ALL_ACCESS, &hKey);

	if (lResult != ERROR_SUCCESS) 
	{
	    if (lResult == ERROR_FILE_NOT_FOUND) {
	        MessageBoxW(nullptr, L"Unable to add to write startup registry.\nCheck if the program has enough privileges.", L"Error", MB_ICONERROR);
			logger::LogError("Registry key not found while opening startup registry key", __FILE__, __LINE__);
	    	return TRUE;
	    } 
	    else {
	        MessageBoxW(nullptr, L"Unable to add to write startup registry.\nCheck if the program has enough privileges.", L"Error", MB_ICONERROR);
			logger::LogError("Access denied writing to registry.", __FILE__, __LINE__);
	        return FALSE;
	    }
	}
	
	lResult = RegSetValueExW(hKey, L"Consoleidator", NULL, REG_SZ, (LPBYTE)wPathToImage, sizeof wPathToImage);

	if (lResult != ERROR_SUCCESS) 
	{
        MessageBoxW(nullptr, L"Unable to add to write startup registry.\nCheck if the program has enough privileges.", L"Error", MB_ICONERROR);
		logger::LogError("Access denied writing to registry.", __FILE__, __LINE__);
        return FALSE;
	}

	return TRUE;
}

BOOL RemoveFromStartup()
{
	WCHAR wPathToImage[500]{};
	GetModuleFileNameW(nullptr, wPathToImage, 500);

	HKEY hKey{};
	LRESULT lResult = 
		RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", NULL, KEY_ALL_ACCESS, &hKey);

	if (lResult != ERROR_SUCCESS) 
	{
	    if (lResult == ERROR_FILE_NOT_FOUND) {
	        MessageBoxW(nullptr, L"Unable to delete from startup registry.\nCheck if the program has enough privileges.", L"Error", MB_ICONERROR);
			logger::LogError("Registry key not found while opening startup registry key", __FILE__, __LINE__);
	    	return TRUE;
	    } 
	    else {
	        MessageBoxW(nullptr, L"Unable to delete from startup registry.\nCheck if the program has enough privileges.", L"Error", MB_ICONERROR);
			logger::LogError("Access denied writing to registry.", __FILE__, __LINE__);
	        return FALSE;
	    }
	}

	lResult = RegDeleteValueW(hKey, L"Consoleidator");

	if (lResult != ERROR_SUCCESS) 
	{
		if (lResult == ERROR_FILE_NOT_FOUND) {
			MessageBoxW(nullptr, L"Unable to delete from startup registry.\nIt probably wasn't present.\nCheck the log for more information.", L"Error", MB_ICONERROR);
			logger::LogError("Unable to delete value \"Consoleidator\" due to reason: ERROR_FILE_NOT_FOUND", __FILE__, __LINE__);
			return FALSE;
		} else {
			MessageBoxW(nullptr, L"Unable to delete from startup registry.\nCheck if the program has enough privileges.", L"Error", MB_ICONERROR);
			logger::LogError("Access denied writing to registry.", __FILE__, __LINE__);
			return FALSE;
		}
        return FALSE;
	}

	return TRUE;
}

void ToggleStartup(HWND hWnd)
{
	HMENU hmenu = GetMenu(hWnd);

	MENUITEMINFO menuItem{};
	menuItem.cbSize = sizeof MENUITEMINFO;
	menuItem.fMask = MIIM_STATE;

	GetMenuItemInfo(hmenu, ID_OPTIONS_RUNATSTARTUP, FALSE, &menuItem);

	if (menuItem.fState == MFS_CHECKED) {
		// Checked, uncheck it
		menuItem.fState = RemoveFromStartup() ? MFS_UNCHECKED : MFS_CHECKED;
	} else {
		// Unchecked, check it
		menuItem.fState = AddToStartup() ? MFS_CHECKED : MFS_UNCHECKED;
	}

	SetMenuItemInfo(hmenu, ID_OPTIONS_RUNATSTARTUP, FALSE, &menuItem);
}

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

void SetStartupCheckmark(HWND hWnd, bool state)
{
	HMENU hmenu = GetMenu(hWnd);

	MENUITEMINFO menuItem{};
	menuItem.cbSize = sizeof MENUITEMINFO;
	menuItem.fMask = MIIM_STATE;

	GetMenuItemInfo(hmenu, ID_OPTIONS_RUNATSTARTUP, FALSE, &menuItem);

	menuItem.fState = state ? MFS_CHECKED : MFS_UNCHECKED;

	SetMenuItemInfo(hmenu, ID_OPTIONS_RUNATSTARTUP, FALSE, &menuItem);
}

BOOL IsStartup()
{
	HKEY hKey{};
	LRESULT lResult = 
		RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", NULL, KEY_ALL_ACCESS, &hKey);

	if (lResult != ERROR_SUCCESS) 
	{
	    if (lResult == ERROR_FILE_NOT_FOUND) {
			MessageBoxW(nullptr, L"Unable to check for startup presence in the registry. The startup button has been disabled.", L"Error", MB_ICONWARNING);
			logger::LogError("Registry key not found while opening startup registry key", __FILE__, __LINE__);
	    	return TRUE;
	    } 
	    else {
	    	MessageBoxW(nullptr, L"Unable to check for startup presence in the registry. The startup button has been disabled.", L"Error", MB_ICONWARNING);
			// disable the startup checkbox
			logger::LogError("Access denied writing to registry.", __FILE__, __LINE__);
	        return FALSE;
	    }
	}

	lResult = RegQueryValueExW(
		hKey,
		L"Consoleidator",
		nullptr,
		nullptr,
		nullptr,
		nullptr
	);

	if (lResult != ERROR_SUCCESS)
	{
		if (lResult == ERROR_FILE_NOT_FOUND) {
			return FALSE;
		} else {
			logger::LogError("An error occured while checking for startup presence in the registry.", __FILE__, __LINE__);
			return FALSE;
		}
	}
	 return TRUE;
}
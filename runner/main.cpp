#include <Windows.h>
#include <cstdio>
#include <string>
#include "resource.h"
#include "runner-constants.hpp"

#define RELEASE

NOTIFYICONDATA nid;
bool isShowing{false};
HMODULE payload_base{};
HINSTANCE hInstance_;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
WPARAM ConvertToMessage(WPARAM wParam);
HHOOK SetKeyboardHook();
BOOL ToggleTray(HWND, HINSTANCE hInstance);
BOOL InjectDllIntoForeground(unsigned int uiMode);

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
    // Register the window class.
    constexpr wchar_t CLASS_NAME[]  = L"Consoleidator";
    
    WNDCLASS wc{};

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    RegisterClass(&wc);

    // Create the window.
    HWND hWnd = CreateWindowEx(
        0,                             
        CLASS_NAME,                    
        L" ",
        WS_OVERLAPPEDWINDOW,           
        0, 0, 400, 200,
        nullptr,
        nullptr,
        hInstance,
        nullptr
        );

    if (hWnd == nullptr)
    {
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    ShowWindow(hWnd, SW_HIDE);

    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, 
        nullptr, 
        PAGE_READWRITE, 
        0, 
        sizeof(HANDLE), 
        "Local\\HotKeyParser"
    );

    if (hMapFile == nullptr)
    {
        MessageBox(nullptr, (L"Failed to create keyboard hook file mapping" + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
	    return false;
    }

    const auto mappedhWnd = (HWND*)MapViewOfFile(
        hMapFile,
		FILE_MAP_WRITE,
		0,
		0,
		sizeof(HWND)
    );

    if (mappedhWnd == nullptr)
    {
	    MessageBox(nullptr, (L"Failed to map keyboard hook file mapping" + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
	    return false;
    }

    *mappedhWnd = hWnd;
    hInstance_ = hInstance;

    if (SetKeyboardHook() == nullptr) return 0;

    CloseHandle(hMapFile);
    UnmapViewOfFile(mappedhWnd);

    // after all setup logic is done successfully, then load the tray icon
    if (!ToggleTray(hWnd, hInstance)) 
    {
        MessageBox(nullptr, L"Failed to add to tray", L"Error", MB_ICONERROR);
    	return 0;
    }

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

BOOL ToggleTray(HWND hWnd, HINSTANCE hInstance)
{
    if (!isShowing)
    {
	    wchar_t szTip[128] = L"Consoleidator";
		nid.cbSize = sizeof NOTIFYICONDATA;
		nid.hWnd = hWnd;
		nid.uID = 100;
		nid.uVersion = NOTIFYICON_VERSION;
		nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wcscpy_s(nid.szTip, 128, szTip);
		nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYNOTIFY;
		return Shell_NotifyIcon(NIM_ADD, &nid);
    }
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		AnimateWindow(hWnd, 100, AW_HIDE | AW_VER_POSITIVE | AW_SLIDE | AW_HOR_POSITIVE);
        isShowing = false;
		ToggleTray(hWnd, hInstance_);
        return 0;		
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			FillRect(hdc, &ps.rcPaint, reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1)));
			EndPaint(hWnd, &ps);
            return 0;
		}
	case WM_TRAYNOTIFY:
		{
            if (lParam == 512) return 0;
			if (lParam == WM_RBUTTONUP)
			{
                HMENU hMenu = CreatePopupMenu();

                wchar_t szShow[] = L"Show";
                wchar_t szExit[] = L"Exit";
				MENUITEMINFO mii;
                mii.cbSize = sizeof(MENUITEMINFO);
				mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
				mii.fState = MFS_ENABLED;
				mii.fType = MFT_STRING;
				mii.wID = ID__SHOW;
                mii.dwTypeData = szShow;
				mii.cch = sizeof szShow;

                InsertMenuItem(hMenu, ID__SHOW, FALSE, &mii);

                mii.wID = ID__EXIT;
                mii.dwTypeData = szExit;
				mii.cch = sizeof szExit;

				InsertMenuItem(hMenu, ID__EXIT, FALSE, &mii);
				SetMenuDefaultItem(hMenu, ID__SHOW, false);
                POINT cursor{};
				GetCursorPos(&cursor);
                SetForegroundWindow(hWnd); // a hack to make the menu disappear when the mouse is clicked outside of it, windows requires it
				if (TrackPopupMenuEx(hMenu, TPM_LEFTALIGN, cursor.x, cursor.y,hWnd, nullptr) == 0)
				{
					MessageBox(hWnd, (L"Failed to show menu: " + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
                    return 0;
				}
			}
            return 0;
		}
	case WM_COMMAND:
		{
			const auto wmID = LOWORD(wParam);
            const auto wmEvent = HIWORD(wParam);
            switch (wmID)
			{
            case ID__SHOW:
				isShowing = true;
				ShowWindow(hWnd, SW_SHOW);
                ToggleTray(hWnd, hInstance_);
                return 0;
            case ID__EXIT:
				PostQuitMessage(0);
                return 0;
            default:
                return 0;
            }
		}
	case WM_PROCESS_KEY:
		{
			const auto keyMsg = ConvertToMessage(wParam);
			switch (keyMsg)
			{
            case HOTKEY_TOGGLE_SHOW:
                if (!isShowing)
                {
                    isShowing = true;
	                ShowWindow(hWnd, SW_SHOW);
                    ToggleTray(hWnd, hInstance_);
                }
				else 
                {
                    isShowing = false;
                	ShowWindow(hWnd, SW_HIDE);
                    ToggleTray(hWnd, hInstance_);
                }
				return 0;
            case HOTKEY_CLEAR_CONSOLE:
            	InjectDllIntoForeground(MODE_CLEAR_CONSOLE);
				return 0;
			case HOTKEY_MAXIMIZE_BUFFER:
				InjectDllIntoForeground(MODE_MAXIMIZE_BUFFER);
				return 0;
            case HOTKEY_RESET_INVERT:
                InjectDllIntoForeground(MODE_RESET_INVERT);
                return 0;
			case HOTKEY_RESET:
				InjectDllIntoForeground(MODE_RESET);
				return 0;
			case HOTKEY_FOREGROUND_FORWARD:
				InjectDllIntoForeground(CYCLE_FOREGROUND_FORWARD);
				return 0;
			case HOTKEY_FOREGROUND_BACKWARD:
				InjectDllIntoForeground(CYCLE_FOREGROUND_BACKWARD);
				return 0;
			case HOTKEY_BACKGROUND_FORWARD:
				InjectDllIntoForeground(CYCLE_BACKGROUND_FORWARD);
				return 0;
			case HOTKEY_BACKGROUND_BACKWARD:
				InjectDllIntoForeground(CYCLE_BACKGROUND_BACKWARD);
				return 0;
			default:
				return 0;
			}
			return 0;
		}
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

BOOL InjectDllIntoForeground(unsigned int uiMode)
{
    DWORD process_id{};
    HWND hForeground = GetForegroundWindow();
    GetWindowThreadProcessId(hForeground, &process_id);

    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
		nullptr,
		PAGE_READWRITE,
		0,
		sizeof(unsigned int),
		"Local\\InjectorMapFile"
        );

    if (hMapFile == nullptr)
	{
        MessageBox(nullptr, (L"Failed to create file mapping" + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
		return EXIT_FAILURE;
	}

    const auto lpuiCallMode = (unsigned int*)MapViewOfFile(
        hMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(unsigned int)
    );


    if (lpuiCallMode == nullptr)
    {
	    MessageBox(nullptr, (L"Failed to map view of file" + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
        return EXIT_FAILURE;
    }

    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        process_id
    );

    *lpuiCallMode = uiMode;

    if (hProcess == nullptr)
    {
	    MessageBox(nullptr, (L"Error while opening foreground process: " + std::to_wstring(GetLastError())).c_str(), L"Error while opening process", MB_ICONERROR);
        return EXIT_FAILURE;
    }
#ifdef RELEASE
    char payloadPath[MAX_PATH]{};
    GetFullPathNameA(payloadNameA, MAX_PATH, payloadPath, nullptr);
#endif

	//this is here because of visual studios stupid run environment
#ifndef RELEASE
#ifndef _DEBUG
    char payloadPath[MAX_PATH]{R"(F:\prj\C++\ConsoleUtilSuite\x64\Release\consoleidator-injectable.dll)"};
#endif
#ifdef _DEBUG
    char payloadPath[MAX_PATH]{R"(F:\prj\C++\ConsoleUtilSuite\x64\Debug\consoleidator-injectable.dll)"};
#endif
#endif

    void* lib_remote = VirtualAllocEx(hProcess, nullptr, __builtin_strlen(payloadPath), MEM_COMMIT, PAGE_READWRITE);
    if (lib_remote == nullptr)
    {
	    MessageBox(nullptr, (L"Error while allocating memory in remote process: " + std::to_wstring(GetLastError())).c_str(), L"Error while allocating in remote", MB_ICONERROR);
        return EXIT_FAILURE;
    }

    HMODULE hKernel32 = GetModuleHandle(L"Kernel32");
    if (hKernel32 == nullptr)
    {
        MessageBox(nullptr, (L"Error while getting module handle of kernel32.dll: " + std::to_wstring(GetLastError())).c_str(), L"Error while getting module handle", MB_ICONERROR);
	    return EXIT_FAILURE;
    }

    const BOOL result = WriteProcessMemory(hProcess, lib_remote, payloadPath, __builtin_strlen(payloadPath), nullptr);
    if (result == FALSE)
    {
    	MessageBox(nullptr, (L"Error while writing to process memory of foreground window: " + std::to_wstring(GetLastError())).c_str(), L"Error while writing to process", MB_ICONERROR);
	    return EXIT_FAILURE;
    }

	HANDLE hThread = CreateRemoteThread(
        hProcess, 
        nullptr, 
        0, 
        reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(hKernel32, "LoadLibraryA")),
        lib_remote, 
        0, 
        nullptr
        );

    if (hThread == nullptr)
	{
    	MessageBox(nullptr, (L"Error while creating remote thread: " + std::to_wstring(GetLastError())).c_str(), L"Error while creating thread", MB_ICONERROR);
	    return EXIT_FAILURE;
	}

    WaitForSingleObject(hThread, INFINITE);
    if (!VirtualFreeEx(hProcess, lib_remote, 0, MEM_RELEASE)) {
        MessageBox(nullptr, (L"Error while freeing memory in remote process: " + std::to_wstring(GetLastError())).c_str(), L"Error while freeing memory", MB_ICONERROR);
	    return EXIT_FAILURE;
    }

    UnmapViewOfFile(hMapFile);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    CloseHandle(hMapFile);
    return EXIT_SUCCESS;
}

// returns nullptr if failed
HHOOK SetKeyboardHook()
{
    static HINSTANCE hHookDll{};

    //load KeyboardProc dll
    hHookDll = LoadLibrary(hookDllNameW);
    if (hHookDll == nullptr)
		return nullptr;

    //get functions memory address
    HOOKPROC hookProc = reinterpret_cast<HOOKPROC>(GetProcAddress(hHookDll, "KeyboardProc"));
    return SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, hHookDll, NULL);
}

/*
 * return 0 for bitmask if NO modifier key is pressed, otherwise returns bitmask of pressed modifier keys
 * parses the states into a bitmask in (almost?) the same way as windows does
 *
 *	    // SHIFT - first bit (0x0001)
 *      // CONTROL - second bit (0x0002)
 *      // ALT - third bit (0x0004)
*/
WPARAM ConvertToMessage(WPARAM wParam)
{
    SHORT bitmask{};

	bitmask |= ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) == 0x00008000);
	bitmask |= ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) == 0x00008000) << 1;
	bitmask |= ((GetAsyncKeyState(VK_LMENU) & 0x8000) == 0x00008000) << 2;
    if (bitmask == 0) return 0;
	for (int i = 0; i < sizeof registeredCombos / sizeof Keycombo; ++i)
	{
		if (wParam == registeredCombos[i].vkCode && bitmask == registeredCombos[i].bitmask)
			return registeredCombos[i].message;
	}
    return 0;
}

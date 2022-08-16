// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <Windows.h>
#include <CommCtrl.h>
#include <cstdio>
#include <string>
#include "Updater.hpp"
#include "resource.h"
#include "runner-constants.hpp"

HWND hWndTitle{};
HWND hWndEdit{};

std::wstring WorkingDirectoryW{};
std::string WorkingDirectoryA{};
NOTIFYICONDATA nid;
bool isShowing{false};
HMODULE payload_base{};
HINSTANCE hInstance_;
Intent* mappedIntent;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProcTitle(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubClass, DWORD_PTR dwRefData);
WPARAM ConvertToMessage(WPARAM wParam);
HHOOK SetKeyboardHook();
BOOL ToggleTray(HWND, HINSTANCE hInstance);
BOOL InjectDllIntoForeground(unsigned int uiMode, const wchar_t* optArg = nullptr);
BOOL ShowTitleSetter();

std::string to_ascii(const std::wstring& str)
{
	std::string ret;
	ret.reserve(str.length());
	for (const auto& c : str)
	{
		ret += static_cast<char>(c);
	}
	return ret;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
#ifdef _DEBUG
    #define _CRTDBG_MAP_ALLOC
	#include <stdlib.h>
	#include <crtdbg.h>
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
	
	const Updater updater("v0.4.0");
	WorkingDirectoryW = updater.ImageDirectory;
	WorkingDirectoryA = to_ascii(WorkingDirectoryW);

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
        L"Consoleidator",
        WS_OVERLAPPEDWINDOW,           
        275, 150, 400, 200,
        nullptr,
        LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1)),
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
        sizeof Intent, 
        "Local\\InjectableMap"
    );

    if (hMapFile == nullptr)
    {
        MessageBox(nullptr, (L"Failed to create keyboard hook file mapping" + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
	    return 0;
    }

    mappedIntent = (Intent*)MapViewOfFile(
        hMapFile,
		FILE_MAP_WRITE,
		0,
		0,
		sizeof(Intent)
    );

    if (mappedIntent == nullptr)
    {
	    MessageBox(nullptr, (L"Failed to map keyboard hook file mapping" + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
	    return 0;
    }

    mappedIntent->loadIntent = true;
    mappedIntent->hWnd = hWnd;
    hInstance_ = hInstance;

    if (SetKeyboardHook() == nullptr) return 0;

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

    // free up resources
    CloseHandle(hMapFile);
    UnmapViewOfFile(mappedIntent);
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
	case WM_CREATE:
		{
			WNDCLASS wnd{};

		    constexpr wchar_t CLASS_NAME[] = L"Consoleidator title setter";
		    
		    wnd.hInstance = hInstance_;
		    wnd.hIcon = LoadIcon(hInstance_, MAKEINTRESOURCE(IDI_ICON1));
		    wnd.lpszClassName = CLASS_NAME;
		    wnd.lpfnWndProc = WindowProcTitle;
		    wnd.style = CS_HREDRAW | CS_VREDRAW;
		    wnd.hbrBackground = CreateSolidBrush(0x2b2b2b);

		    if (RegisterClass(&wnd) == NULL)
		    {
			    MessageBox(nullptr, L"Failed to register title setter window class!", L"Error", MB_ICONERROR);
		        return 0;
		    }

		    hWndTitle = CreateWindowEx(
		        WS_EX_TOOLWINDOW,
		        CLASS_NAME,
		        CLASS_NAME,
		        0,
		        CW_USEDEFAULT,
		        CW_USEDEFAULT,
		        350,
		        20,
		        nullptr,
		        nullptr,
		        nullptr,
		        nullptr
		        );

		    if (hWndTitle == nullptr)
		    {
			    MessageBox(nullptr, L"Failed to create title setter window", L"Error", MB_ICONERROR);
		        return 0;
		    }

		    // hack to remove the title bar
		    SetWindowLongPtr(hWndTitle, GWL_STYLE, 0);

		    hWndEdit = CreateWindow
			(
		        L"EDIT",
		        L"",
		        WS_CHILD | WS_BORDER | WS_VISIBLE | ES_LEFT,
		        0, 0,
		        350, 20,
		        hWndTitle,
		        nullptr,
		        nullptr,
		        nullptr
		        );

			// initialize NONCLIENTMETRICS structure
			NONCLIENTMETRICS ncm;
			ncm.cbSize = sizeof(ncm);

			// obtain non-client metrics for the default system font
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

			// create the new font
			HFONT hNewFont = CreateFontIndirect(&ncm.lfMessageFont);

			// set the new font
			SendMessage(hWndEdit, WM_SETFONT, (WPARAM)hNewFont, 0);

			// subclass the edit control
			if (SetWindowSubclass(hWndEdit, EditSubclassProc, 0, 0) == 0)
			{
				MessageBox(nullptr, L"Failed to subclass edit control", L"Error", MB_ICONERROR);
				return 0;
			}

			return 0;
		}
	case WM_CLOSE:
        isShowing = false;
        ShowWindow(hWnd, SW_HIDE);
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
            if (lParam == WM_LBUTTONDBLCLK)
            {
	            isShowing = true;
				ShowWindow(hWnd, SW_SHOW);
                ToggleTray(hWnd, hInstance_);
                return 0;
            }

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
				SetMenuDefaultItem(hMenu, ID__SHOW, FALSE);
                POINT cursor{};
				GetCursorPos(&cursor);
                SetForegroundWindow(hWnd); // a hack to make the menu disappear when the mouse is clicked outside it, windows requires it
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
                    SetForegroundWindow(hWnd);
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
			case HOTKEY_SET_TITLE:
				// this shows the title setting window at the foreground title bar
				// the injection event is handled when the user presses return
                ShowTitleSetter();
				return 0;
			default:
				return 0;
			}
			return 0;
		}
	case WM_USER_UPDATE_COMPLETE:
		{
			MessageBoxW(hWnd, L"Update was completed successfully!", L"Update successful", MB_ICONINFORMATION);
			return 0;
		}
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubClass, DWORD_PTR dwRefData)
{
	switch (uMsg)
	{
	case WM_KEYUP:
		{
			switch (wParam)
			{
			case VK_RETURN:
				{
					wchar_t str[MAX_PATH * 4]{};
					GetWindowTextW(hWndEdit, str, MAX_PATH * 4);
					ShowWindow(hWndTitle, SW_HIDE);
					InjectDllIntoForeground(MODE_SET_TITLE, str);
					return 0;
				}
			default:
				return 0;
			}
		}
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			ShowWindow(hWndTitle, SW_HIDE);
			return 0;
		}
	default:
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}	
}

LRESULT CALLBACK WindowProcTitle(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
    case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		FillRect(hdc, &ps.rcPaint, CreateSolidBrush(0x2b2b2b));
		EndPaint(hWnd, &ps);
	    return 0;
	}
	case WM_CTLCOLOREDIT:
	    {
			SetBkColor((HDC)wParam, RGB(43, 43, 43));
			SetTextColor((HDC)wParam, RGB(255, 255, 255));
			return (LPARAM)CreateSolidBrush(0x2b2b2b);
	    }
	case WM_COMMAND:
    {
	    if (HIWORD(wParam) == EN_KILLFOCUS)
	    {
		    ShowWindow(hWndEdit, SW_HIDE);
			ShowWindow(hWndTitle, SW_HIDE);
			return 0;
	    }
		return 0;
    }
	default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

// optional argument used to transfer title to the dll
BOOL InjectDllIntoForeground(unsigned int uiMode, const wchar_t * optArg)
{
    DWORD process_id{};
    HWND hForeground = GetForegroundWindow();
    GetWindowThreadProcessId(hForeground, &process_id);

    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        process_id
    );
    mappedIntent->loadIntent = false;
    mappedIntent->uiMode = uiMode;

	// copy title to dll if available
	if (optArg != nullptr) wcscpy_s(mappedIntent->title, optArg);

    if (hProcess == nullptr)
    {
	    MessageBox(nullptr, (L"Error while opening foreground process: " + std::to_wstring(GetLastError())).c_str(), L"Error while opening process", MB_ICONERROR);
        return EXIT_FAILURE;
    }

    char payloadPath[MAX_PATH]{};
	memcpy(payloadPath, WorkingDirectoryA.data(), WorkingDirectoryA.size());
    memcpy(payloadPath + WorkingDirectoryA.size(), payloadNameA, __builtin_strlen(payloadNameA));

    void* lib_remote = VirtualAllocEx(hProcess, nullptr, __builtin_strlen(payloadPath), MEM_COMMIT, PAGE_READWRITE);
    if (lib_remote == nullptr)
    {
	    MessageBox(nullptr, (L"Error while allocating memory in remote process: " + std::to_wstring(GetLastError())).c_str(), L"Error while allocating in remote", MB_ICONERROR);
		CloseHandle(hProcess);
        return EXIT_FAILURE;
    }

    HMODULE hKernel32 = GetModuleHandle(L"Kernel32");
    if (hKernel32 == nullptr)
    {
        MessageBox(nullptr, (L"Error while getting module handle of kernel32.dll: " + std::to_wstring(GetLastError())).c_str(), L"Error while getting module handle", MB_ICONERROR);
		CloseHandle(hProcess);
	    return EXIT_FAILURE;
    }

	size_t written{};

    const BOOL result = WriteProcessMemory(hProcess, lib_remote, payloadPath, __builtin_strlen(payloadPath), &written);
    if (result == FALSE)
    {
    	MessageBox(nullptr, (L"Error while writing to process memory of foreground window: " + std::to_wstring(GetLastError())).c_str(), L"Error while writing to process", MB_ICONERROR);
		CloseHandle(hProcess);
	    return EXIT_FAILURE;
    }

	assert(written == __builtin_strlen(payloadPath));

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
		CloseHandle(hProcess);
    	MessageBox(nullptr, (L"Error while creating remote thread: " + std::to_wstring(GetLastError())).c_str(), L"Error while creating thread", MB_ICONERROR);
	    return EXIT_FAILURE;
	}

    WaitForSingleObject(hThread, INFINITE);
    if (!VirtualFreeEx(hProcess, lib_remote, 0, MEM_RELEASE))
	{
		CloseHandle(hProcess);
        MessageBox(nullptr, (L"Error while freeing memory in remote process: " + std::to_wstring(GetLastError())).c_str(), L"Error while freeing memory", MB_ICONERROR);
	    return EXIT_FAILURE;
    }
	
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return EXIT_SUCCESS;
}

BOOL ShowTitleSetter()
{
    RECT anchor{};
	wchar_t title[MAX_PATH * 4]{};

	// retrieve the handle to the foreground window
    HWND hWndForeground = GetForegroundWindow();

	// retrieve anchor position 
    GetWindowRect(hWndForeground, &anchor);

	// retrieve the actual window title of the target foreground window and append it to the control
	GetWindowTextW(hWndForeground, title, MAX_PATH * 4);
	SetWindowTextW(hWndEdit, title);

	const WPARAM len = wcslen(title);

	SendMessage(hWndEdit, EM_SETSEL, len, len);

	// show the actual edit window in title bar
    SetWindowPos(hWndTitle, HWND_TOPMOST, anchor.left + 32, anchor.top + 2, anchor.right - anchor.left - 185, 28, SWP_SHOWWINDOW);
	SetWindowPos(hWndEdit, nullptr, 0, 4, anchor.right - anchor.left - 4, 24, SWP_NOZORDER | SWP_SHOWWINDOW);

	// set keyboard focus to edit control
    if (SetFocus(hWndTitle) == nullptr)
    {
	    MessageBox(nullptr, (L"Cannot set foreground, error: " + std::to_wstring(GetLastError())).c_str(), L"Error while setting foreground", MB_ICONERROR);
		return EXIT_FAILURE;
    }
	if (SetFocus(hWndEdit) == nullptr)
	{
	    MessageBox(nullptr, L"Error while setting focus to edit control", L"Error while setting focus", MB_ICONERROR);
		return EXIT_FAILURE;
	}
	return TRUE;
}


// Sets the low-level keyboard hook defined in consoleidator-injectable/payload-main.cpp
HHOOK SetKeyboardHook()
{
    static HINSTANCE hHookDll{};

    //load KeyboardProc dll
    hHookDll = LoadLibraryW(payloadNameW);
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
// TODO: port to the Windows.h defined modifier macros
WPARAM ConvertToMessage(WPARAM wParam)
{
    SHORT bitmask{};

	bitmask |= ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) == 0x00008000);
	bitmask |= ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) == 0x00008000) << 1;
	bitmask |= ((GetAsyncKeyState(VK_LMENU) & 0x8000) == 0x00008000) << 2;
    if (bitmask == 0) return 0;
	for (int i = 0; i < sizeof registeredCombos / sizeof Keycombo; ++i)
	{
		if (wParam == (WPARAM)registeredCombos[i].vkCode && bitmask == registeredCombos[i].bitmask)
			return registeredCombos[i].message;
	}
    return 0;
}

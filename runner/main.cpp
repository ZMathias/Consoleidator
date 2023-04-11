// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <Windows.h>
#include <CommCtrl.h>
#include <cstdio>
#include <string>
#include "Updater.hpp"
#include "runner-constants.hpp"
#include "logger.hpp"
#include "startup.hpp"
#include "accent_map.hpp"
#include "resource.h"

HWND hWndTitle{};
HWND hWndEdit{};

int buildNumber{};
std::wstring WorkingDirectoryW{};
std::string WorkingDirectoryA{};
NOTIFYICONDATA nid;
bool isShowing{false};
HMODULE payload_base{};
HINSTANCE hInstance_;
MemoryMapDescriptor* sharedMemoryStruct;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProcTitle(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubClass, DWORD_PTR dwRefData);
WPARAM ConvertToMessage(WPARAM key);
HHOOK SetKeyboardHook();
BOOL ToggleTray(HWND, HINSTANCE hInstance);
BOOL IsConsoleWindow(HWND hWnd);
BOOL InjectDllIntoWindow(unsigned int uiMode, HWND hForeground, const wchar_t* optArg = nullptr);
BOOL ShowTitleSetter();
std::string ToAscii(const std::wstring& str);
bool DoesLayoutHaveDeadKeys();

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
#ifdef _DEBUG
    #define _CRTDBG_MAP_ALLOC
	#include <cstdlib>
	#include <crtdbg.h>
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
	
	const Updater updater("v0.5.6");
	WorkingDirectoryW = updater.ImageDirectory;
	WorkingDirectoryA = ToAscii(WorkingDirectoryW);

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
		const std::string error = "Failed to create Consoleidator window";
		MessageBoxA(nullptr, error.c_str(), "Fatal error", MB_ICONERROR);
		logger::LogError(error, __FILE__, __LINE__);
        return 0;
    }

    ShowWindow(hWnd, SW_HIDE);

	// set up a file mapping for communicating with the injectable DLL
	// the dll connects to this "Local\\InjectableMap"
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, 
        nullptr, 
        PAGE_READWRITE, 
        0, 
        sizeof MemoryMapDescriptor, 
        "Local\\InjectableMap"
    );

    if (hMapFile == nullptr)
    {
        MessageBox(nullptr, (L"Failed to create keyboard hook file mapping" + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
		logger::LogError("Failed to create keyboard hook file mapping", __FILE__, __LINE__);
	    return 0;
    }

    sharedMemoryStruct = (MemoryMapDescriptor*)MapViewOfFile(
        hMapFile,
		FILE_MAP_WRITE,
		0,
		0,
		sizeof MemoryMapDescriptor
    );

    if (sharedMemoryStruct == nullptr)
    {
	    MessageBox(nullptr, (L"Failed to map keyboard hook file mapping" + std::to_wstring(GetLastError())).c_str(), L"Error", MB_ICONERROR);
		logger::LogError("Failed to map keyboard hook file mapping", __FILE__, __LINE__);
	    return 0;
    }

	ZeroMemory(sharedMemoryStruct, sizeof MemoryMapDescriptor);

	
    sharedMemoryStruct->hookIntent = true;
    sharedMemoryStruct->hWnd = hWnd;

	// a copy of hInstance as a global variable used in the message loop
    hInstance_ = hInstance;

	// these are fatal conditions
	// if these don't work the application has no purpose
	const HHOOK keyboardHook = SetKeyboardHook();
    if (keyboardHook == nullptr) 
    {
		logger::LogError("Failed to set keyboard hook", __FILE__, __LINE__);
    	return 0;
	}

    // after all setup logic is done successfully, then load the tray icon
    if (!ToggleTray(hWnd, hInstance)) 
    {
        MessageBox(nullptr, L"Failed to add to tray", L"Error", MB_ICONERROR);
		logger::LogError("Failed to add to tray", __FILE__, __LINE__);
    	return 0;
    }

	// all is set-up
	// we initialise the accent table and merge it with the configurable table
	
	int res = MergeAccentMaps(WorkingDirectoryW);

	switch (res)
	{
	case -1:
		// error case, display message box
		// we fall back to the default accent table
		break;
	case 0:
		// no accent table found, fall back to default
		break;
	case 1:
		// accent table merged with config file, copy of original is kept
		break;
	default:
		// should never happen
		break;
	}


	// TODO: convert to a normal array for IPC
	// TODO: move the code snippet up and make the shared memory allocation dynamic

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // free up resources
    CloseHandle(hMapFile);
    UnmapViewOfFile(sharedMemoryStruct);
	UnhookWindowsHookEx(keyboardHook);
    return 0;
}

std::string ToAscii(const std::wstring& str)
{
	std::string ret;
	ret.reserve(str.length());
	for (const auto& c : str)
	{
		ret += static_cast<char>(c);
	}
	return ret;
}

BOOL ToggleTray(HWND hWnd, HINSTANCE hInstance)
{
	// set tray properties and toggle showing state based on current window state 'isShowing'
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
	// window proc for main application window
	switch (uMsg)
	{
	case WM_CREATE:
		{
			// create the title setting window and hide it on startup
			// we do it this way to minimize resource use and latency when the title setting window is invoked
			WNDCLASS wnd{};

		    constexpr wchar_t CLASS_NAME[] = L"Consoleidator title setter";
		    
		    wnd.hInstance = hInstance_;
		    wnd.hIcon = LoadIcon(hInstance_, MAKEINTRESOURCE(IDI_ICON1));
		    wnd.lpszClassName = CLASS_NAME;
		    wnd.lpfnWndProc = WindowProcTitle;
		    wnd.style = CS_HREDRAW | CS_VREDRAW;

			// we try to match the Windows 11 dark theme here
			// TODO: have to add some theming based on windows versions
		    wnd.hbrBackground = CreateSolidBrush(0x2b2b2b);

		    if (RegisterClass(&wnd) == NULL)
		    {
			    MessageBox(nullptr, L"Failed to register title setter window class!", L"Error", MB_ICONERROR);
				logger::LogError("Failed to register title setter window class!", __FILE__, __LINE__);
		        return 0;
		    }

			// default window size, will be overwritten in ShowTitleSetter() to match the in-focus windows' title bar size
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

			// initialize text field
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

			// subclass the edit control to capture input loss and detailed key press info to hide the title setter window when needed
			if (SetWindowSubclass(hWndEdit, EditSubclassProc, 0, 0) == 0)
			{
				MessageBox(nullptr, L"Failed to subclass edit control", L"Error", MB_ICONERROR);
				logger::LogError("Failed to subclass edit control", __FILE__, __LINE__);
				return 0;
			}

			IsStartup() ? SetStartupCheckmark(hWnd, true) : SetStartupCheckmark(hWnd, false);

			return 0;
		}
	case WM_CLOSE:
        isShowing = false;
        ShowWindow(hWnd, SW_HIDE);
		ToggleTray(hWnd, hInstance_);
        return 0;		
	case WM_DESTROY:
		ToggleTray(hWnd, hInstance_);
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
			// handle tray icon click
            if (lParam == 512) return 0;

			// show the title setter window
            if (lParam == WM_LBUTTONDBLCLK)
            {
	            isShowing = true;
				ShowWindow(hWnd, SW_SHOW);
                ToggleTray(hWnd, hInstance_);
                return 0;
            }

			// show the context menu
			if (lParam == WM_RBUTTONUP)
			{
                HMENU hMenu = CreatePopupMenu();

				// we build the menu dynamically, not as a resource
				// it was less of a hassle at the time of writing this
                wchar_t szShow[] = L"Show";
                wchar_t szExit[] = L"Exit";
				wchar_t szRestart[] = L"Restart";
				MENUITEMINFO mii;
                mii.cbSize = sizeof(MENUITEMINFO);
				mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
				mii.fState = MFS_ENABLED;
				mii.fType = MFT_STRING;
				mii.wID = ID__SHOW;
                mii.dwTypeData = szShow;
				mii.cch = sizeof szShow;
				InsertMenuItem(hMenu, ID__SHOW, FALSE, &mii);

                mii.wID = ID__RESTART;
                mii.dwTypeData = szRestart;
                mii.cch = sizeof szRestart;
                InsertMenuItem(hMenu, ID__RESTART, FALSE, &mii);

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
					const std::string error{("Failed to show menu: " + std::to_string(GetLastError()))};
					MessageBoxA(hWnd, error.c_str(), "Error", MB_ICONERROR);
					logger::LogError(error, __FILE__, __LINE__);
                    return 0;
				}
			}
            return 0;
		}
	case WM_COMMAND:
		// we process the menu commands here
		{
			const auto wmID = LOWORD(wParam);
            const auto wmEvent = HIWORD(wParam);
            switch (wmID)
			{
            	// hide the window when we click X
            case ID__SHOW:
				isShowing = true;
				ShowWindow(hWnd, SW_SHOW);
                ToggleTray(hWnd, hInstance_);
                return 0;
            case ID__EXIT:
				PostQuitMessage(0);
                return 0;
            case ID__RESTART:
	            Updater::Restart();
				return 0;
            case ID_OPTIONS_RUNATSTARTUP:
	            {
            		ToggleStartup(hWnd);
	            }
            default:
                return 0;
            }
		}
		// self defined window message WM_USER + 1 = 1025
		// used to handle messages generated by the LowLevelKeyboard hook
	case WM_PROCESS_KEY:
		{
			HWND hForeground = GetForegroundWindow();
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
				if(IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(MODE_CLEAR_CONSOLE, hForeground);
				}
				return 0;
			case HOTKEY_MAXIMIZE_BUFFER:
				if (IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(MODE_MAXIMIZE_BUFFER, hForeground);
				}
				return 0;
            case HOTKEY_RESET_INVERT:
				if (IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(MODE_RESET_INVERT, hForeground);
				}
                return 0;
			case HOTKEY_RESET:
				if (IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(MODE_RESET, hForeground);
				}
				return 0;
			case HOTKEY_FOREGROUND_FORWARD:
				if (IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(CYCLE_FOREGROUND_FORWARD, hForeground);
				}
				return 0;
			case HOTKEY_FOREGROUND_BACKWARD:
				if (IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(CYCLE_FOREGROUND_BACKWARD, hForeground);
				}
				return 0;
			case HOTKEY_BACKGROUND_FORWARD:
				if (IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(CYCLE_BACKGROUND_FORWARD, hForeground);
				}
				return 0;
			case HOTKEY_BACKGROUND_BACKWARD:
				if (IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(CYCLE_BACKGROUND_BACKWARD, hForeground);
				}
				return 0;
			case HOTKEY_HELP:
				if (IsConsoleWindow(hForeground)) {
					InjectDllIntoWindow(MODE_HELP, hForeground);
				}
				return 0;
			case HOTKEY_SET_TITLE:
				// this shows the title setting window at the foreground title bar
				// the injection event is handled when the user presses return
				// in the EditSubclassProc function
                ShowTitleSetter();
				return 0;
			default:
				return 0;
			}
			return 0;
		}
	case WM_COPYDATA:
		// this is used the handle the copied over keybuffer from the low-level keyhook
		// we decide here if:
		// 1. the hotkey conditions are met => CTRL + SPACE is pressed
		// 2. the last two characters are eligible for accent replacement
		// finally we replace the characters and return true to intercept the SPACE keypress from the keyhook
		{
			static bool deadkeyAlert = false;
			const auto pcds = reinterpret_cast<COPYDATASTRUCT*>(lParam);

			// we check if the data originates from the keyhook
			// this value is a constant defined in both the keyhook and the main apps constants header
			// we assign this constant to the dwData field in the keyhook to indicate the message originates from the keyhook
			if (pcds->dwData != WM_COPYDATA_VERIFICATION)
			{
				return DefWindowProc(hWnd, uMsg, wParam, lParam);
			}

			// since WM_COPYDATA is only sent when space is pressed, we can assume the keypress is VK_SPACE
			const auto msg = ConvertToMessage(VK_SPACE);

			if (msg != HOTKEY_ACCENT_REPLACE)
			{
				// we return false to not hook the SPACE keypress
				return false;
			}

			if (!deadkeyAlert && DoesLayoutHaveDeadKeys())
			{
				deadkeyAlert = true;
				MessageBoxW(
					nullptr,
					L"The current keyboard layout contains dead-keys. It is advised to switch to a keyboard layout that doesn't have dead keys or define other combinations for accented characters, that don't use said dead keys. For example: German keyboard layout contains the \"�\" dead key. It only appears after pressing the key twice or combining it with another character to produce an accented character. For example pressing \"�\" and then \"a\" produces \"�\". This obviously interferes with the purpose of Consoleidator. The program can be used going further but will require using the shortcut twice successively.",
					L"Consoleidator dead key warning",
					MB_ICONWARNING
					);
				return false;
			}


			// the lpData field is always a pointer to a character array
			const std::string translatedBuffer = (char*)pcds->lpData;

			logger::LogInfo("accent replace invoked, buffer contents: " + translatedBuffer, __FILE__, __LINE__);

			// safety measure to prevent segfaults
			if (translatedBuffer.length() < 2)
			{
				return false;
			}

			// we assemble the key combo
			std::string combo;
			combo += translatedBuffer[translatedBuffer.size() - 2];
			combo += translatedBuffer[translatedBuffer.size() - 1];
	

			if (!accentMap.contains(combo))
			{
				// this combo is not registered
				return false;
			}


			wchar_t composed = accentMap[combo];
			// 8 is the total number of keyboard events to be sent
			// CTRL UP - SPACE UP - BACKSPACE - BACKSPACE UP - BACKSPACE - BACKSPACE UP - ACCENT CHAR - ACCENT CHAR UP
			INPUT inputs[8]{};
			ZeroMemory(inputs, sizeof inputs);

			inputs[0].type = INPUT_KEYBOARD;
			inputs[0].ki.wVk = VK_CONTROL;
			inputs[0].ki.dwFlags = KEYEVENTF_KEYUP;

			inputs[1].type = INPUT_KEYBOARD;
			inputs[1].ki.wVk = VK_SPACE;
			inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

			inputs[2].type = INPUT_KEYBOARD;
			inputs[2].ki.wVk = VK_BACK;
			inputs[2].ki.dwFlags = 0;

			inputs[3].type = INPUT_KEYBOARD;
			inputs[3].ki.wVk = VK_BACK;
			inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

			inputs[4].type = INPUT_KEYBOARD;
			inputs[4].ki.wVk = VK_BACK;
			inputs[4].ki.dwFlags = 0;

			inputs[5].type = INPUT_KEYBOARD;
			inputs[5].ki.wVk = VK_BACK;
			inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

			inputs[6].type = INPUT_KEYBOARD;
			inputs[6].ki.wScan = composed;
			inputs[6].ki.wVk = 0;
			inputs[6].ki.dwFlags = KEYEVENTF_UNICODE;

			inputs[7].type = INPUT_KEYBOARD;
			inputs[7].ki.wScan = composed;
			inputs[7].ki.wVk = 0;
			inputs[7].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_UNICODE;

			UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));

			return true;			
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
					// handle injection when user presses ENTER
					// we need to get the text from the edit control before we inject
					wchar_t str[MAX_PATH * 4]{};
					GetWindowTextW(hWndEdit, str, MAX_PATH * 4);
					ShowWindow(hWndTitle, SW_HIDE);
					InjectDllIntoWindow(MODE_SET_TITLE, GetForegroundWindow(), str);
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

	// TODO: set up theming for title-setter based on windows versions
	//
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
			// hide title setter when focus is lost
			// BUG: doesn't always work since microsoft restricted how ShowWindow works.
			// BUG: When the ShowWindow is called, the window doesn't always force itself to the foreground and grab keyboard focus, therefore there won't be a killfocus message generated, since no focus was ever present in the first place.
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

BOOL IsConsoleWindow(HWND hWnd)
{
	wchar_t className[MAX_PATH]{};
	GetClassNameW(hWnd, className, MAX_PATH);
	return wcscmp(className, L"ConsoleWindowClass") == 0;
}

// optional argument used to copy window title to the dll
BOOL InjectDllIntoWindow(unsigned int uiMode, HWND hForeground, const wchar_t * optArg)
{
    DWORD process_id{};
    GetWindowThreadProcessId(hForeground, &process_id);

    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        process_id
    );

	// set all state data needed for the injection in the mapped memory region
    sharedMemoryStruct->hookIntent = false;
    sharedMemoryStruct->uiMode = uiMode;

	// copy title to dll if available
	if (optArg != nullptr) wcscpy_s(sharedMemoryStruct->title, optArg);

    if (hProcess == nullptr)
    {
		const std::string error = "Error while opening foreground process: " + std::to_string(GetLastError());
	    MessageBoxA(nullptr, error.c_str(), "Error while opening process", MB_ICONERROR);
		logger::LogError("Error while opening foreground process: " + std::to_string(GetLastError()), __FILE__, __LINE__);
        return EXIT_FAILURE;
    }

	// isn't unicode compatible
	// BUG: LoadLibraryW refuses to get called, but LoadLibraryA does get called
    char payloadPath[MAX_PATH]{};
	memcpy(payloadPath, WorkingDirectoryA.data(), WorkingDirectoryA.size());
    memcpy(payloadPath + WorkingDirectoryA.size(), payloadNameA, __builtin_strlen(payloadNameA));

    void* lib_remote = VirtualAllocEx(hProcess, nullptr, __builtin_strlen(payloadPath), MEM_COMMIT, PAGE_READWRITE);
    if (lib_remote == nullptr)
    {
		const std::string error = "Error while allocating memory in foreground process: " + std::to_string(GetLastError());
	    MessageBoxA(nullptr, error.c_str(), "Error while allocating in remote", MB_ICONERROR);
		logger::LogError("Error while allocating memory in foreground process: " + std::to_string(GetLastError()), __FILE__, __LINE__);
		CloseHandle(hProcess);
        return EXIT_FAILURE;
    }

    HMODULE hKernel32 = GetModuleHandle(L"Kernel32");
    if (hKernel32 == nullptr)
    {
		const std::string error = "Error while getting kernel32 handle: " + std::to_string(GetLastError());
        MessageBoxA(nullptr, error.c_str(), "Error while getting module handle", MB_ICONERROR);
		logger::LogError("Error while getting kernel32 handle: " + std::to_string(GetLastError()), __FILE__, __LINE__);
		CloseHandle(hProcess);
	    return EXIT_FAILURE;
    }

	size_t written{};

    const BOOL result = WriteProcessMemory(hProcess, lib_remote, payloadPath, __builtin_strlen(payloadPath), &written);
    if (result == FALSE)
    {
		const std::string error = "Error while writing to foreground process: " + std::to_string(GetLastError());
		MessageBoxA(nullptr, error.c_str(), "Error while writing to remote", MB_ICONERROR);
		logger::LogError(error, __FILE__, __LINE__);
		CloseHandle(hProcess);
	    return EXIT_FAILURE;
    }

	// assert that we've written the path correctly, otherwise abort because injection will fail and most likely
	// lead to a crash or destablize the injected app
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
		const std::string error = "Error while creating remote thread: " + std::to_string(GetLastError());
    	MessageBoxA(nullptr, error.c_str(), "Error while creating thread", MB_ICONERROR);
		CloseHandle(hProcess);
	    return EXIT_FAILURE;
	}

    WaitForSingleObject(hThread, INFINITE);
    if (!VirtualFreeEx(hProcess, lib_remote, 0, MEM_RELEASE))
	{
		const std::string error = "Error while freeing memory in foreground process: " + std::to_string(GetLastError());
        MessageBoxA(nullptr, error.c_str(), "Error while freeing memory", MB_ICONERROR);
		logger::LogError(error, __FILE__, __LINE__);
		CloseHandle(hProcess);
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

	// check if console window
	if (!IsConsoleWindow(hWndForeground)) return FALSE;

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
		const std::string error = "Error while setting focus to title window: " + std::to_string(GetLastError());
		MessageBoxA(nullptr, error.c_str(), "Error while setting focus", MB_ICONERROR);
		logger::LogError(error, __FILE__, __LINE__);
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
    HINSTANCE hHookDll{};

	// load DLL
	// if it is already loaded then it retrieves the handle to it
    hHookDll = LoadLibraryW(payloadNameW);
    if (hHookDll == nullptr)
		return nullptr;

    //get functions memory address
    HOOKPROC hookProc = reinterpret_cast<HOOKPROC>(GetProcAddress(hHookDll, "KeyboardProc"));
    return SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, hHookDll, NULL);
}

/*
 * return 0 if unable to convert to a hotkey message
 * otherwise returns the corresponding hotkey message for example: HOTKEY_SET_TITLE
 * parses the states into a bitmask in (almost?) the same way as windows does
 *
 *	    // SHIFT - first bit (0x0001)
 *      // CONTROL - second bit (0x0002)
 *      // ALT - third bit (0x0004)
*/
// TODO: port to the Windows.h defined modifier macros
WPARAM ConvertToMessage(WPARAM key)
{
    SHORT bitmask{};

	bitmask |= (GetAsyncKeyState(VK_LSHIFT) & 0x8000) == 0x00008000;
	bitmask |= ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) == 0x00008000) << 1;
	bitmask |= ((GetAsyncKeyState(VK_LMENU) & 0x8000) == 0x00008000) << 2;
    if (bitmask == 0) return 0;
	for (int i = 0; i < sizeof registeredCombos / sizeof Keycombo; ++i)
	{
		if (key == (WPARAM)registeredCombos[i].vkCode && bitmask == registeredCombos[i].bitmask)
			return registeredCombos[i].message;
	}
    return 0;
}


bool DoesLayoutHaveDeadKeys()
{
	BYTE kbState[256]{};
	GetKeyboardState(kbState);
	HKL layout = GetKeyboardLayout(0);

	for (UINT i = 0; i < 256; ++i)
	{
		kbState[i] = 0x80;
		WCHAR chr{};
		int result = ToUnicodeEx(
			i, 
			MapVirtualKeyEx(i, MAPVK_VK_TO_VSC, layout), 
			kbState, 
			&chr, 
			1,
			2,
			layout
		);

		if (result == -1)
		{
			return true;
		}
		kbState[i] = 0;
	}
	return false;
}

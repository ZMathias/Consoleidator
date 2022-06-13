#include <Windows.h>
#include <cstdio>
#include <string>
#include "runner-constants.hpp"

bool isShowing{false};
HMODULE payload_base{};

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL InjectDllIntoForeground(unsigned int uiMode);

//bool RegisterHotkeys(HWND hWnd);
HHOOK SetKeyboardHook();
WPARAM ConvertToMessage(WPARAM wParam);

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
    // Register the window class.
    constexpr wchar_t CLASS_NAME[]  = L" ";
    
    WNDCLASS wc = { };

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the window.

    HWND hWnd = CreateWindowEx(
        0,                             
        CLASS_NAME,                    
        L" ",
        WS_OVERLAPPEDWINDOW,           
        0, 0, 10, 10,
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

    if (SetKeyboardHook() == nullptr) return 0;
    //if (RegisterHotkeys(hWnd) == false) return 0;

    CloseHandle(hMapFile);
    UnmapViewOfFile(mappedhWnd);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
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
	case WM_PROCESS_KEY:
		{
			const auto keyMsg = ConvertToMessage(wParam);
			switch (keyMsg)
			{
            case HOTKEY_TOGGLE_SHOW:
            	isShowing = !isShowing;
				ShowWindow(hWnd, SW_SHOW * isShowing);
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

    char payloadPath[MAX_PATH]{};
    GetFullPathNameA(payloadNameA, MAX_PATH, payloadPath, nullptr);

    // this is here because of visual studios stupid run environment
//#ifndef _DEBUG
//    char payloadPath[MAX_PATH]{R"(F:\prj\C++\ConsoleUtilSuite\x64\Release\consoleidator-injectable.dll)"};
//#endif
//#ifdef _DEBUG
//    char payloadPath[MAX_PATH]{R"(F:\prj\C++\ConsoleUtilSuite\x64\Debug\payload.dll)"};
//#endif

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
	HOOKPROC hookProc{};
    static HINSTANCE hHookDll{};

    //load keyboardproc dll
    hHookDll = LoadLibrary(hookDllNameW);
    if (hHookDll == nullptr)
		return nullptr;

    //get functions memory address
    hookProc = reinterpret_cast<HOOKPROC>(GetProcAddress(hHookDll, "KeyboardProc"));
    return SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, hHookDll, 0);
}

WPARAM ConvertToMessage(WPARAM wParam)
{
    SHORT bitmask{};

	// parses the states into a bitmask in (almost) the same way as windows does
	// SHIFT - first bit (0x0001)
	// CONTROL - second bit (0x0002)
	// ALT - third bit (0x0004)
	//precast the bitmask to avoid casting it three times

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

/*bool RegisterHotkeys(HWND hWnd)
{
    // if already bound, then another instance is already running. Abort.
	if (RegisterHotKey(hWnd, HOTKEY_TOGGLE_SHOW, MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, VK_F5) == 0) return false;
    RegisterHotKey(hWnd, HOTKEY_CLEAR_CONSOLE, MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, VK_DELETE);
	RegisterHotKey(hWnd, HOTKEY_MAXIMIZE_BUFFER, MOD_SHIFT | MOD_CONTROL | MOD_NOREPEAT, 0x4D);
    RegisterHotKey(hWnd, HOTKEY_RESET, MOD_SHIFT | MOD_CONTROL | MOD_NOREPEAT, VK_END);

    //hotkey for cycling through the foreground cmd colors with CTRL + SHIFT + LEFT/RIGHTARROW
    RegisterHotKey(hWnd, HOTKEY_FOREGROUND_BACKWARD, MOD_SHIFT | MOD_CONTROL | MOD_NOREPEAT, VK_LEFT);
    RegisterHotKey(hWnd, HOTKEY_FOREGROUND_FORWARD, MOD_SHIFT | MOD_CONTROL | MOD_NOREPEAT, VK_RIGHT);

    //hotkey for cycling through the background cmd colors with CTRL + SHIFT + UP/DOWNARROW
    RegisterHotKey(hWnd, HOTKEY_BACKGROUND_BACKWARD, MOD_SHIFT | MOD_CONTROL | MOD_NOREPEAT, VK_DOWN);
	RegisterHotKey(hWnd, HOTKEY_BACKGROUND_FORWARD, MOD_SHIFT | MOD_CONTROL | MOD_NOREPEAT, VK_UP);
    return true;
}*/

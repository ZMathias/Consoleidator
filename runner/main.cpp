#include <Windows.h>
#include <cstdio>
#include <string>

constexpr int INVALID_ARG_ERROR = 87;

constexpr WPARAM HOTKEY_TOGGLE_SHOW = 1;
constexpr WPARAM HOTKEY_CLEAR_CONSOLE = 2;

bool isShowing{false};
HMODULE payload_base{};

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool MaximizeConsoleBuffer(const HANDLE&);
bool ClearConsole(const HANDLE&);
bool InjectDllIntoForeground(HMODULE& payload_base);
bool DetachDllFromForeground(const HMODULE& payload_base);

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
    RegisterHotKey(hWnd, HOTKEY_TOGGLE_SHOW, MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, VK_F5);
    RegisterHotKey(hWnd, HOTKEY_CLEAR_CONSOLE,MOD_SHIFT | MOD_CONTROL | MOD_NOREPEAT, VK_DELETE);
    ShowWindow(hWnd, SW_HIDE);

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
	case WM_HOTKEY:
		{
			switch (wParam)
			{
            case HOTKEY_TOGGLE_SHOW:
				{
					isShowing = !isShowing;
				}
				ShowWindow(hWnd, SW_SHOW * isShowing);
				return 0;
            case HOTKEY_CLEAR_CONSOLE:
            	InjectDllIntoForeground(payload_base);
				return 0;
			default:
                return 0;
			}
		}
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

bool InjectDllIntoForeground(HMODULE& payload_base)
{
    DWORD process_id{};
    HWND hForeground = GetForegroundWindow();
    GetWindowThreadProcessId(hForeground, &process_id);
    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        process_id
    );

    if (hProcess == nullptr)
    {
	    MessageBox(nullptr, (L"Error while opening foreground process: " + std::to_wstring(GetLastError())).c_str(), L"Error while opening process", MB_ICONERROR);
        return EXIT_FAILURE;
    }
    //char payloadPath[MAX_PATH]{};
    //GetFullPathNameA("payload.dll", MAX_PATH, payloadPath, nullptr);

    char payloadPath[] = R"(F:\prj\C++\ConsoleUtilSuite\x64\Release\payload.dll)";

    void* lib_remote = VirtualAllocEx(hProcess, nullptr, strlen(payloadPath), MEM_COMMIT, PAGE_READWRITE);
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

    const BOOL result = WriteProcessMemory(hProcess, lib_remote, payloadPath, sizeof payloadPath, nullptr);
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

    // get the HMODULE of the loaded library
    DWORD exit_code{};
    GetExitCodeThread(hThread, &exit_code);
    payload_base = reinterpret_cast<HMODULE>(exit_code);

    if (!VirtualFreeEx(hProcess, lib_remote, 0, MEM_RELEASE)) {
        MessageBox(nullptr, (L"Error while freeing memory in remote process: " + std::to_wstring(GetLastError())).c_str(), L"Error while freeing memory", MB_ICONERROR);
	    return EXIT_FAILURE;
    }

    CloseHandle(hThread);
    CloseHandle(hProcess);
    return EXIT_SUCCESS;
}

bool MaximizeConsoleBuffer(const HANDLE& hStdOut)
{
	
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo{};
	GetConsoleScreenBufferInfo(hStdOut, &bufferInfo);

	const SHORT xSize{bufferInfo.dwSize.X};
	constexpr SHORT ySize{32766};

	if (!SetConsoleScreenBufferSize(hStdOut, {xSize, ySize}))
	{
		const auto error = GetLastError();
		if (error == INVALID_ARG_ERROR) printf("invalid argument\n");
		else printf("error setting buffer size: %lu", GetLastError());
		return true;
	}

	printf("\nbefore\nx: %i\ny: %i\n\n", bufferInfo.dwSize.X, bufferInfo.dwSize.Y);
	GetConsoleScreenBufferInfo(hStdOut, &bufferInfo);
	printf("after\nx: %i\ny: %i\n", bufferInfo.dwSize.X, bufferInfo.dwSize.Y);
	return false;
}
// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <string>

constexpr unsigned int MODE_CLEAR_CONSOLE = 1;
constexpr unsigned int MODE_MAXIMIZE_BUFFER = 2;

bool MaximizeConsoleBuffer(const HANDLE& hStdOut)
{
	
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo{};
	GetConsoleScreenBufferInfo(hStdOut, &bufferInfo);

	const SHORT xSize{bufferInfo.dwSize.X};
	constexpr SHORT ySize{32766};

	if (!SetConsoleScreenBufferSize(hStdOut, {xSize, ySize}))
	{
		return false;
	}

	printf("\nbefore\nx: %i\ny: %i\n\n", bufferInfo.dwSize.X, bufferInfo.dwSize.Y);
	GetConsoleScreenBufferInfo(hStdOut, &bufferInfo);
	printf("after\nx: %i\ny: %i\n", bufferInfo.dwSize.X, bufferInfo.dwSize.Y);
	return true;
}

bool ClearConsole(const HANDLE& hStdOut)
{
	// Fetch existing console mode so we correctly add a flag and not turn off others
    DWORD mode = 0;
    if (!GetConsoleMode(hStdOut, &mode)) return EXIT_FAILURE;

    // Hold original mode to restore on exit to be cooperative with other command-line apps.
    const DWORD originalMode = mode;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(hStdOut, mode)) return EXIT_FAILURE;

    // Write the sequence for clearing the display.
    DWORD written = 0;
    constexpr PCWSTR sequence = L"\x1b[2J";
    if (!WriteConsoleW(hStdOut, sequence, static_cast<DWORD>(wcslen(sequence)), &written, nullptr))
    {
        // If we fail, try to restore the mode on the way out.
        SetConsoleMode(hStdOut, originalMode);
        return EXIT_FAILURE;
    }

    // To also clear the scroll back, emit L"\x1b[3J" as well.
    // 2J only clears the visible window and 3J only clears the scroll back.
	constexpr PCWSTR scrollSequence = L"\x1b[3J";
	if (!WriteConsoleW(hStdOut, scrollSequence, static_cast<DWORD>(wcslen(scrollSequence)), &written, nullptr))
	{
		SetConsoleMode(hStdOut, originalMode);
		return EXIT_FAILURE;
	}
	
    // Restore the mode on the way out to be nice to other command-line applications.
    SetConsoleMode(hStdOut, originalMode);
	SetConsoleCursorPosition(hStdOut, {0, 0});
	return EXIT_SUCCESS;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
	    {
            auto hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    		HANDLE hMapFile = OpenFileMappingA(
				FILE_MAP_ALL_ACCESS, 
				FALSE, 
				"Local\\InjectorMapFile"
			);
    		if (hMapFile == nullptr)
    		{
    			const std::wstring strError = L"Failed to open mapped file: " + std::to_wstring(GetLastError());
    			WriteConsole(hStdOut, strError.c_str(), (DWORD)strError.size(), nullptr, nullptr);
    			return FALSE;
    		}

    		auto lpuiMode = (unsigned int*)MapViewOfFile(
				hMapFile,
				FILE_MAP_ALL_ACCESS,
				0,
				0,
				sizeof(HANDLE)
			);

    		if (lpuiMode == nullptr)
    		{
    			const std::wstring strError = L"Failed to map view of file: " + std::to_wstring(GetLastError());
    			WriteConsole(hStdOut, strError.c_str(), (DWORD)strError.size(), nullptr, nullptr);
                return FALSE;
    		}

            switch (*lpuiMode)
            {
	            case MODE_CLEAR_CONSOLE:
		            ClearConsole(hStdOut);
		            break;
                case MODE_MAXIMIZE_BUFFER:
					MaximizeConsoleBuffer(hStdOut);
					break;
    		default:
				break;
            }
            break;
	    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return FALSE;
}


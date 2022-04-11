// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <string>

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
    //std::string str = "Error while loading: " + std::to_string(GetLastError());
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ClearConsole(GetStdHandle(STD_OUTPUT_HANDLE));
		//WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str.c_str(), str.length(), nullptr, nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return FALSE;
}


#include "pch.h"
#include <string>
#include <cstdlib>
#include "constants.hpp"

bool FillConsoleOutputAttributeAndSet(const HANDLE& hStdOut, const WORD& wAttributes, const DWORD& dwLength)
{
	constexpr COORD beginning{0, 0};
	DWORD charsWritten{};
	if (SetConsoleTextAttribute(hStdOut, wAttributes) == 0) return false;
	if (FillConsoleOutputAttribute(hStdOut, wAttributes, dwLength, beginning, &charsWritten) == 0) return false;
	return true;	
}

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
    constexpr PCWSTR sequence = L"\x1b[2J\x1b[3J";
    if (!WriteConsoleW(hStdOut, sequence, static_cast<DWORD>(wcslen(sequence)), &written, nullptr))
    {
        // If we fail, try to restore the mode on the way out.
        SetConsoleMode(hStdOut, originalMode);
        return EXIT_FAILURE;
    }


	SetConsoleCursorPosition(hStdOut, {0, 0});

	if (!WriteConsoleW(hStdOut, sequence, static_cast<DWORD>(wcslen(sequence)), &written, nullptr))
    {
        // If we fail, try to restore the mode on the way out.
        SetConsoleMode(hStdOut, originalMode);
        return EXIT_FAILURE;
    }

    // Restore the mode on the way out to be nice to other command-line applications.
    SetConsoleMode(hStdOut, originalMode);
	SetConsoleCursorPosition(hStdOut, {0, 0});
	return EXIT_SUCCESS;
}

bool CycleForeground(const HANDLE& hStdOut, const bool forward)
{
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo{};
	GetConsoleScreenBufferInfo(hStdOut, &bufferInfo);

	if (forward)
	{
		if (FOREGROUND_COMPONENT == 15)
		{
			if (FillConsoleOutputAttributeAndSet(hStdOut, bufferInfo.wAttributes - 15, BUFFER_LENGTH) == 0) return false;
			
			return true;
		}
		else
		{
			if (FillConsoleOutputAttributeAndSet(hStdOut, bufferInfo.wAttributes + 1, BUFFER_LENGTH) == 0) return false;
			return true;
		}
	}
	else
	{
		if (FOREGROUND_COMPONENT == 0)
		{
			if (FillConsoleOutputAttributeAndSet(hStdOut, bufferInfo.wAttributes + 15, BUFFER_LENGTH) == 0) return false;
			return true;
		}
		else
		{
			if (FillConsoleOutputAttributeAndSet(hStdOut, bufferInfo.wAttributes - 1, BUFFER_LENGTH) == 0) return false;
			return true;
		}
	}
	return true;
}

bool CycleBackground(const HANDLE& hStdOut, const bool forward)
{
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo{};
	GetConsoleScreenBufferInfo(hStdOut, &bufferInfo);

	if (forward)
	{
		if (bufferInfo.wAttributes - (bufferInfo.wAttributes & 0x000F) == 0x00F0)
		{
			if (FillConsoleOutputAttributeAndSet(hStdOut, 0x000F & bufferInfo.wAttributes, BUFFER_LENGTH) == 0)
			{
				return false;
			}
			return true;
		}
		else
		{
			if (FillConsoleOutputAttributeAndSet(hStdOut, bufferInfo.wAttributes + 0x0010, BUFFER_LENGTH) == 0) return false;
			return true;
		}
	}
	else
	{
		if (BACKGROUND_COMPONENT == 0)
		{
			if (FillConsoleOutputAttributeAndSet(hStdOut, 0x00F0 + FOREGROUND_COMPONENT, BUFFER_LENGTH) == 0) return false;
			return true;
		}
		else
		{
			if (FillConsoleOutputAttributeAndSet(hStdOut, bufferInfo.wAttributes - 0x0010, BUFFER_LENGTH) == 0) return false;
			return true;
		}
	}
}

bool ResetConsole(const HANDLE& hStdOut, const bool& invert = false)
{
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo{};
	GetConsoleScreenBufferInfo(hStdOut, &bufferInfo);

	if (FillConsoleOutputAttributeAndSet(hStdOut, invert ? 0xF0 : 0x0F, BUFFER_LENGTH) == 0) return false;
	return true;
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

    		const auto lpuiMode = (unsigned int*)MapViewOfFile(
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
    			case MODE_RESET:
					ResetConsole(hStdOut);
					break;
				case MODE_RESET_INVERT:
					ResetConsole(hStdOut, true);
					break;
				case CYCLE_FOREGROUND_FORWARD:
					CycleForeground(hStdOut, true);
					break;
				case CYCLE_FOREGROUND_BACKWARD:
					CycleForeground(hStdOut, false);
					break;
				case CYCLE_BACKGROUND_FORWARD:
					CycleBackground(hStdOut, true);
					break;
				case CYCLE_BACKGROUND_BACKWARD:
					CycleBackground(hStdOut, false);
					break;
    		default:
				break;
            }
            UnmapViewOfFile(hMapFile);
			CloseHandle(hMapFile);
            
            break;
	    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return FALSE;
}


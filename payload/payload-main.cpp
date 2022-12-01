// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include <string>
#include "injectable-constants.hpp"

MemoryMapDescriptor* sharedMemoryStruct;

void pushQueue()
{
	for (size_t i = 0; i < KEY_BUF_LEN - 1; ++i)
	{
		sharedMemoryStruct->keyBuffer[i] = sharedMemoryStruct->keyBuffer[i + 1];
	}
	sharedMemoryStruct->keyBuffer[KEY_BUF_LEN - 1] = {};
}

void popQueue()
{
	for (size_t i = KEY_BUF_LEN - 1; i >= 1; --i)
	{
		sharedMemoryStruct->keyBuffer[i] = sharedMemoryStruct->keyBuffer[i - 1];
	}
	sharedMemoryStruct->keyBuffer[0] = {};
}

bool isAcceptedChar(const DWORD key)
{
	// check if the key is a number, letter special characters like hyphens and underscores
	// space and return are also accepted
	// check the ranges at: https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	return  (key >= 0x30 && key <= 0x39) || // ASCII 0-9
			(0x41 <= key && key <= 0x5A) || // ASCII A-Z
			(0x60 <= key && key <= 0x6F) || // ASCII 0-9 NUMPAD and NUMPAD +-*/.
			(0xBA <= key && key <= 0xE2) || // ASCII ;=,-./`[]\'
			key == VK_RETURN || key == VK_SPACE || key == VK_BACK;
}

// we send the virtual key-code of a key that is watched by this function
extern "C" __declspec(dllexport) LRESULT KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
        const auto key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

		static bool shiftDown = false;
		static bool capsToggled = false;

		// we must store modifier key states in order to properly handle capitals and special characters
		// this is probably the easiest way to do and it integrates well with the win32 ToUnicode function
		if ((key->vkCode == VK_LSHIFT || key->vkCode == VK_RSHIFT) && 
			(wParam == WM_KEYFIRST || wParam == WM_KEYLAST || wParam == WM_KEYUP))
		{
			shiftDown = !shiftDown;
		}

		if (key->vkCode == VK_CAPITAL && wParam == WM_KEYUP)
		{
			capsToggled = !capsToggled;
		}

        if (wParam == WM_KEYDOWN)
        {
			// we only store ascii representable keys in the key buffer
			// this is to prevent the buffer from being filled with control keys that dont end up in a text field
			// this simplifies checking for a specific decomposed accented key combination
			if (isAcceptedChar(key->vkCode))
			{
				// treat it as a queue
				// the most recent key is at the end of the array
				if (key->vkCode == VK_BACK)
				{
					popQueue();
				}
				else
				{
					pushQueue();
					sharedMemoryStruct->keyBuffer[KEY_BUF_LEN - 1].vkCode = key->vkCode;
					sharedMemoryStruct->keyBuffer[KEY_BUF_LEN - 1].scanCode = key->scanCode;
					sharedMemoryStruct->keyBuffer[KEY_BUF_LEN - 1].time = key->time;
					sharedMemoryStruct->keyBuffer[KEY_BUF_LEN - 1].shiftDown = shiftDown;
					sharedMemoryStruct->keyBuffer[KEY_BUF_LEN - 1].capsToggled = capsToggled;
				}
			}
        	for (int i = 0; i < sizeof registeredKeys / sizeof DWORD; ++i)
	        {
                if (registeredKeys[i] == key->vkCode)
                {
					// the intent structure also contains the HWND for our main window
					// this allows us to send messages for processing in nice fashion
	                SendMessage(sharedMemoryStruct->hWnd, WM_PROCESS_KEY, (WPARAM)key->vkCode, 0);
                }
	        }
        }
	}
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void PrintHelp(HANDLE hStdOut)
{
	const std::string help =
		"\n----------------------------------------------------\n"
		"CTRL + SHIFT + DELETE - Clear console\n"
		"CTRL + SHIFT + M - Maximize console buffer\n"
		"CTRL + SHIFT + T - Set console title\n"
		"CTRL + RIGHT - Cycle foreground color forward\n"
		"CTRL + LEFT - Cycle foreground color backward\n"
		"CTRL + UP - Cycle background color forward\n"
		"CTRL + DOWN - Cycle background color backward\n"
		"CTRL + END - Reset console colors to white on black\n"
		"CTRL + HOME - Reset console colors to black on white\n"
		"----------------------------------------------------\n";
	printf("%s", help.c_str());
}

bool FillConsoleOutputAttributeAndSet(HANDLE hStdOut, WORD wAttributes, DWORD dwLength)
{
	constexpr COORD beginning{0, 0};
	DWORD charsWritten{};
	if (SetConsoleTextAttribute(hStdOut, wAttributes) == 0) return false;
	if (FillConsoleOutputAttribute(hStdOut, wAttributes, dwLength, beginning, &charsWritten) == 0) return false;
	return true;	
}

bool MaximizeConsoleBuffer(HANDLE hStdOut)
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

bool ClearConsole(HANDLE hStdOut)
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

bool CycleForeground(HANDLE hStdOut, const bool forward)
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

bool CycleBackground(const HANDLE hStdOut, const bool forward)
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

bool ResetConsole(HANDLE hStdOut, const bool invert = false)
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
				"Local\\InjectableMap"
			);
    		if (hMapFile == nullptr)
    		{
    			const std::wstring strError = L"Failed to open mapped file: " + std::to_wstring(GetLastError());
    			WriteConsole(hStdOut, strError.c_str(), (DWORD)strError.size(), nullptr, nullptr);
    			return FALSE;
    		}

    		sharedMemoryStruct = (MemoryMapDescriptor*)MapViewOfFile(
				hMapFile,
				FILE_MAP_ALL_ACCESS,
				0,
				0,
				sizeof(MemoryMapDescriptor)
			);

    		if (sharedMemoryStruct == nullptr)
    		{
    			const std::wstring strError = L"Failed to map view of file: " + std::to_wstring(GetLastError());
    			WriteConsole(hStdOut, strError.c_str(), (DWORD)strError.size(), nullptr, nullptr);
                return FALSE;
    		}

			// we must return true for SetWindowsHookEx to succeed
			// but only when we load actually set the DLL
			// we need to check for that intention here
			if (sharedMemoryStruct->loadIntent) return TRUE;

            switch (sharedMemoryStruct->uiMode)
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
	            case MODE_SET_TITLE:
					SetConsoleTitle(sharedMemoryStruct->title);
					break;
	            case MODE_HELP:
					PrintHelp(hStdOut);
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

	// we always return false to avoid keeping the dll in memory
	// this way it signals to Windows that it can unload the dll
    return FALSE;
}


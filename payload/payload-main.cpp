// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include <string>
#include "injectable-constants.hpp"

MemoryMapDescriptor* sharedMemoryStruct;
bool shiftDown = false;
bool controlDown = false;
bool capsToggled = false;
bool capsTransition = false;
KeyDescriptor keyBuffer[KEY_BUF_LEN]{}; // initialize to zero

void PushBuffer(KeyDescriptor key)
{
	for (size_t i = 0; i < KEY_BUF_LEN - 1; ++i)
	{
		keyBuffer[i] = keyBuffer[i + 1];
	}
	keyBuffer[KEY_BUF_LEN - 1] = key;
}

void PopBuffer()
{
	for (size_t i = KEY_BUF_LEN - 1; i > 0; --i)
	{
		keyBuffer[i] = keyBuffer[i - 1];
	}
	keyBuffer[0] = {};
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
			key == VK_SPACE || key == VK_BACK;
}

std::string translateBuffer()
{
	std::string translatedBuffer;
	translatedBuffer.reserve(KEY_BUF_LEN);

	// convert the buffer to a character string with ToAscii and check for eligible pairs
	// if eligible, intercept the keypress and we call our parent process for handling the character replacement
	BYTE* keyState = new BYTE[256]{};
	for (size_t i = 0; i < KEY_BUF_LEN; ++i)
	{
		if (keyBuffer[i].vkCode == NULL)
		{
			continue;
		}

		// set the keyboard state according to MSDN
		// every key that is pressed has to have the MSB set
		// every key that has a toggle state (caps, numlock, scrolllock) has to have the LSB set if it is toggled
		keyState[VK_SHIFT] = keyBuffer[i].shiftDown << 7;
		keyState[VK_CONTROL] = keyBuffer[i].controlDown << 7;
		keyState[VK_CAPITAL] = keyBuffer[i].capsToggled;

		WORD ch{};
		const int res = ToAscii(
			keyBuffer[i].vkCode,
			keyBuffer[i].scanCode,
			keyState,
			&ch,
			0
		);

		switch (res)
		{
		case 0:
			// implement logging
			break;
		case 1:
			// implement logging
			translatedBuffer += static_cast<char>(ch);
			break;
		case 2:
			// there are two characters stored in the ch variable
			// the ToAscii probably failed to combine a dead key with a character
			// we can't do anything about it, so we just append the characters
			translatedBuffer += *reinterpret_cast<char*>(&ch);
			translatedBuffer += *(reinterpret_cast<char*>(&ch) + 1);
			break;
		default:
			break;
		}
	}
	return translatedBuffer;
}

//std::string convertMessage(WPARAM wParam)
//{
//	switch (wParam)
//	{
//	case VK_LSHIFT:
//		return "LSHIFT";
//	case VK_RSHIFT:
//		return "RSHIFT";
//	case VK_SHIFT:
//		return "SHIFT";
//	case WM_KEYFIRST:
//		return "WM_KEYDOWN/WM_KEYFIRST";
//	case WM_KEYLAST:
//		return "WM_KEYLAST";
//	case WM_KEYUP:
//		return "WM_KEYUP";
//	default:
//		return std::to_string(wParam);
//	}
//}

// we send the virtual key-code of a key that is watched by this function
extern "C" __declspec(dllexport) LRESULT KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
        const auto key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

		if (key->flags & LLKHF_INJECTED)
		{
			return CallNextHookEx(nullptr, code, wParam, lParam);
		}

		//if (key->vkCode == VK_LSHIFT || key->vkCode == VK_RSHIFT || key->vkCode == VK_SHIFT)
		//{
		//	std::ofstream file("msg_log.txt", std::ofstream::app);
		//	file << "vkCode: " << convertMessage(key->vkCode) << " wParam: " << convertMessage(wParam) << '\n';
		//	file.close();
		//}

		// we must store modifier key states in order to properly handle capitals and special characters
		// this is probably the easiest way to do and it integrates well with the win32 ToAscii function
		if ((key->vkCode == VK_LSHIFT || key->vkCode == VK_RSHIFT || key->vkCode == VK_SHIFT))
		{
			shiftDown = wParam == WM_KEYDOWN ? true : false;
		}
		
		if ((key->vkCode == VK_LCONTROL || key->vkCode == VK_RCONTROL || key->vkCode == VK_CONTROL))
		{
			controlDown = wParam == WM_KEYDOWN ? true : false;
		}

		if (key->vkCode == VK_CAPITAL)
		{
			if (wParam == WM_KEYDOWN)
			{
				if (!capsTransition)
				{
					capsToggled = !capsToggled;
					capsTransition = true;
				}
			}
			else
			{
				if (capsTransition)
				{
					capsTransition = false;
				}
			}
		}

        if (wParam == WM_KEYDOWN)
        {
        	// this is for the accent replacer
			// it checks if the key is accepted and appends it to a KEY_BUF_LEN sized buffer of type KeyDescriptor
			// the KeyDescriptor only contains the fields used below
			// we store keystate to be able to handle special characters with ToAscii
			if (isAcceptedChar(key->vkCode))
			{
				switch (key->vkCode)
				{
				case VK_BACK:
					PopBuffer();
					break;
				case VK_SPACE:
					{
						const std::string translatedBuffer = translateBuffer();
						COPYDATASTRUCT cds{};
						cds.dwData = WM_COPYDATA_VERIFICATION;
						cds.cbData = translatedBuffer.size() + 1;
						cds.lpData = (char*)translatedBuffer.c_str();

						// send the buffer contents to our parent process for handling
						// this includes replacing the characters with the accented counterpart
						// if this returns 1, we intercept the keypress and the action is complete
						if (SendMessage(sharedMemoryStruct->hWnd, WM_COPYDATA, 0, (LPARAM)&cds))
						{
							// returning any non-zero value will prevent the keypress from propagating to the rest of the system
							PopBuffer();
							PopBuffer();
							return 1;
						}
					}

					// the fallthrough is intented
					// if there is no replacement, we just append the character to the buffer
				default:
					KeyDescriptor key_D{};
					key_D.vkCode = key->vkCode;
					key_D.scanCode = key->scanCode;
					key_D.time = key->time;
					key_D.shiftDown = shiftDown;
					key_D.controlDown = controlDown;
					key_D.capsToggled = capsToggled;
					PushBuffer(key_D);
					break;
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
			if (sharedMemoryStruct->hookIntent) return TRUE;

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


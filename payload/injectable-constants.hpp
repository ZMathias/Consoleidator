// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include <Windows.h>

#define FOREGROUND_COMPONENT (bufferInfo.wAttributes & 0x000F)
#define BACKGROUND_COMPONENT (bufferInfo.wAttributes & 0x00F0)
#define BUFFER_LENGTH (bufferInfo.dwSize.X * bufferInfo.dwSize.Y)

// length of key buffer queue
constexpr size_t KEY_BUF_LEN = 10;

// max length of console buffer (500 in X (realistic value) and maximum in Y (scrollable))
constexpr unsigned int MAXIMUM_CONSOLE_SIZE = (500 * 32766);

// DLL hooking modes set in the mapped memory
constexpr unsigned int MODE_CLEAR_CONSOLE = 1;
constexpr unsigned int MODE_MAXIMIZE_BUFFER = 2;
constexpr unsigned int CYCLE_FOREGROUND_FORWARD = 3;
constexpr unsigned int CYCLE_FOREGROUND_BACKWARD = 4;
constexpr unsigned int CYCLE_BACKGROUND_FORWARD = 5;
constexpr unsigned int CYCLE_BACKGROUND_BACKWARD = 6;
constexpr unsigned int MODE_RESET = 7;
constexpr unsigned int MODE_RESET_INVERT = 8;
constexpr unsigned int MODE_SET_TITLE = 9;
constexpr unsigned int MODE_HELP = 10;
constexpr unsigned int MODE_READ_CONSOLE_BUFFER = 11;

// wparam constants for receiving the hotkey message
constexpr UINT WM_PROCESS_KEY = WM_USER + 1;

// 0x4D is the ASCII code for the 'M' key
// 0x54 is the ASCII code for the 'T' key
constexpr DWORD VK_M = 0x4D;
constexpr DWORD VK_T = 0x54;
constexpr DWORD VK_H = 0x48;
constexpr DWORD VK_F = 0x46;

// verification constant for WM_COPYDATA
constexpr DWORD WM_COPYDATA_KEYBUFFER_TYPE = 0x4D4F4F43;

constexpr DWORD registeredKeys[] = 
{
	VK_SPACE,
	VK_F5,
	VK_DELETE,
	VK_UP,
	VK_DOWN,
	VK_LEFT,
	VK_RIGHT,
	VK_END,
	VK_HOME,
	VK_M,
	VK_T,
	VK_H,
	VK_F
};

struct KeyDescriptor
{
	DWORD vkCode{};
	DWORD scanCode{};
	DWORD time{};
	bool shiftDown = false;
	bool controlDown = false;
	bool capsToggled = false;
};

struct MemoryMapDescriptor
{
	// this is set when the DLL is used for hooking
	// the SetWindowsHookEx function expects a return TRUE upon LoadLibrary completion
	// but to avoid the DLL being loaded into every process we touch, we always return false
	// except when the DLL is used for hooking, in which case we return true
	bool hookIntent{};

	// stores the current injection mode of the DLL
	// used to determine which action to perform on the console
	unsigned int uiMode{};
	// stores the hWnd of our parent process
	// used to send window messages to it from the hotkey handler
	HWND hWnd{};
	// stores the title copied from the title setter window
	// used to actully set the console title
	wchar_t title[MAX_PATH]{};

	// we use the KeyDescriptor at startup to check initial keystates
	// key states maintained in the hook are only relative to KEYDOWN and KEYUP messages, thus it can be incorrect to assume that, upon startup, every key is in its default state
	KeyDescriptor initKeyState{};

	// stores the text from the console buffer
	// in case the user requests it
	char consoleTextBuffer[MAXIMUM_CONSOLE_SIZE]{};
	volatile unsigned int bufferSize{};
	COORD consoleDimensions{};
};
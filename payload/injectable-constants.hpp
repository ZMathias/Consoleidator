#pragma once
#include <Windows.h>

#define FOREGROUND_COMPONENT (bufferInfo.wAttributes & 0x000F)
#define BACKGROUND_COMPONENT (bufferInfo.wAttributes & 0x00F0)
#define BUFFER_LENGTH (bufferInfo.dwSize.X * bufferInfo.dwSize.Y)

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

// wparam constants for receiving the hotkey message
constexpr UINT WM_PROCESS_KEY = WM_USER + 1;

// 0x4D is the ASCII code for the 'M' key
// 0x54 is the ASCII code for the 'T' key
constexpr DWORD VK_M = 0x4D;
constexpr DWORD VK_T = 0x54;

constexpr DWORD registeredKeys[] = {VK_F5, VK_DELETE, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_END, VK_HOME, VK_M, VK_T};

struct Intent
{
	bool loadIntent{};
	unsigned int uiMode{};
	HWND hWnd{};
};
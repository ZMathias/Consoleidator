#pragma once
#include <Windows.h>
#define FOREGROUND_COMPONENT (bufferInfo.wAttributes & 0x000F)
#define BACKGROUND_COMPONENT (bufferInfo.wAttributes & 0x00F0)
#define BUFFER_LENGTH (bufferInfo.dwSize.X * bufferInfo.dwSize.Y)

// DLL hooking modes set in the mapped memory
constexpr unsigned int MODE_CLEAR_CONSOLE = 1;
constexpr unsigned int MODE_MAXIMIZE_BUFFER = 2;
constexpr unsigned int MODE_RESET = 7;
constexpr unsigned int MODE_RESET_INVERT = 8;
constexpr unsigned int CYCLE_FOREGROUND_FORWARD = 3;
constexpr unsigned int CYCLE_FOREGROUND_BACKWARD = 4;
constexpr unsigned int CYCLE_BACKGROUND_FORWARD = 5;
constexpr unsigned int CYCLE_BACKGROUND_BACKWARD = 6;

// wparam constants for receiving the hotkey message
constexpr UINT WM_PROCESS_KEY = WM_USER + 1;
constexpr WPARAM HOTKEY_TOGGLE_SHOW = 1;
constexpr WPARAM HOTKEY_CLEAR_CONSOLE = 2;
constexpr WPARAM HOTKEY_MAXIMIZE_BUFFER = 3;
constexpr WPARAM HOTKEY_RESET = 8;
constexpr WPARAM HOTKEY_FOREGROUND_FORWARD = 4;
constexpr WPARAM HOTKEY_FOREGROUND_BACKWARD = 5;
constexpr WPARAM HOTKEY_BACKGROUND_FORWARD = 6;
constexpr WPARAM HOTKEY_BACKGROUND_BACKWARD = 7;

const DWORD registeredKeys[] = {VK_F5, VK_DELETE, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_END, VK_HOME, 0x4D};

struct Intent
{
	bool loadIntent{};
	unsigned int uiMode{};
	HWND hWnd;
};
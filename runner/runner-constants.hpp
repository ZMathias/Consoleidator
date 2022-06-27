#pragma once

// wparam constants for receiving the hotkey message
constexpr UINT WM_PROCESS_KEY = WM_USER + 1;
constexpr UINT WM_TRAYNOTIFY = WM_USER + 2;
constexpr WPARAM HOTKEY_TOGGLE_SHOW = 1;
constexpr WPARAM HOTKEY_CLEAR_CONSOLE = 2;
constexpr WPARAM HOTKEY_MAXIMIZE_BUFFER = 3;
constexpr WPARAM HOTKEY_RESET = 8;
constexpr WPARAM HOTKEY_RESET_INVERT = 9;
constexpr WPARAM HOTKEY_FOREGROUND_FORWARD = 4;
constexpr WPARAM HOTKEY_FOREGROUND_BACKWARD = 5;
constexpr WPARAM HOTKEY_BACKGROUND_FORWARD = 6;
constexpr WPARAM HOTKEY_BACKGROUND_BACKWARD = 7;

// DLL hooking modes set in the mapped memory, the uiMode should be one of these values
constexpr unsigned int MODE_CLEAR_CONSOLE = 1;
constexpr unsigned int MODE_MAXIMIZE_BUFFER = 2;
constexpr unsigned int MODE_RESET = 7;
constexpr unsigned int MODE_RESET_INVERT = 8;
constexpr unsigned int CYCLE_FOREGROUND_FORWARD = 3;
constexpr unsigned int CYCLE_FOREGROUND_BACKWARD = 4;
constexpr unsigned int CYCLE_BACKGROUND_FORWARD = 5;
constexpr unsigned int CYCLE_BACKGROUND_BACKWARD = 6;

constexpr wchar_t payloadNameW[] = L"consoleidator-injectable.dll";
constexpr char payloadNameA[] = "consoleidator-injectable.dll";

struct Keycombo
{
	SHORT bitmask{};
    DWORD vkCode{};
	WPARAM message;
};


constexpr int SHIFT_MOD = 0x1;
constexpr int CONTROL_MOD = 0x2;
constexpr int ALT_MOD = 0x4;

constexpr Keycombo registeredCombos[] = {
	{CONTROL_MOD | SHIFT_MOD | ALT_MOD, VK_F5, HOTKEY_TOGGLE_SHOW},
	{CONTROL_MOD | SHIFT_MOD, VK_DELETE, HOTKEY_CLEAR_CONSOLE},
	{CONTROL_MOD | SHIFT_MOD, 0x4D, HOTKEY_MAXIMIZE_BUFFER},
	{CONTROL_MOD, VK_HOME, HOTKEY_RESET_INVERT},
	{CONTROL_MOD, VK_END, HOTKEY_RESET},
	{CONTROL_MOD, VK_RIGHT, HOTKEY_FOREGROUND_FORWARD},
	{CONTROL_MOD, VK_LEFT, HOTKEY_FOREGROUND_BACKWARD},
	{CONTROL_MOD, VK_UP, HOTKEY_BACKGROUND_FORWARD},
	{CONTROL_MOD, VK_DOWN, HOTKEY_BACKGROUND_BACKWARD},
};

struct Intent
{
	bool loadIntent{};
	unsigned int uiMode{};
	HWND hWnd;
};
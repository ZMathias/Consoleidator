﻿// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once

// constants for handling message events generated by the Shell_NotifyIcon
constexpr UINT WM_PROCESS_KEY = WM_USER + 1;
constexpr UINT WM_TRAYNOTIFY = WM_USER + 2;
constexpr UINT WM_USER_UPDATE_COMPLETE = WM_USER + 3;
constexpr UINT WM_MAP_AT = WM_USER + 4;
constexpr UINT WM_MAP_CONTAINS = WM_USER + 5;

// wparam constants for receiving the hotkey message
constexpr WPARAM HOTKEY_TOGGLE_SHOW = 1;
constexpr WPARAM HOTKEY_CLEAR_CONSOLE = 2;
constexpr WPARAM HOTKEY_MAXIMIZE_BUFFER = 3;
constexpr WPARAM HOTKEY_FOREGROUND_FORWARD = 4;
constexpr WPARAM HOTKEY_FOREGROUND_BACKWARD = 5;
constexpr WPARAM HOTKEY_BACKGROUND_FORWARD = 6;
constexpr WPARAM HOTKEY_BACKGROUND_BACKWARD = 7;
constexpr WPARAM HOTKEY_RESET = 8;
constexpr WPARAM HOTKEY_RESET_INVERT = 9;
constexpr WPARAM HOTKEY_SET_TITLE = 10;
constexpr WPARAM HOTKEY_HELP = 11;
constexpr WPARAM HOTKEY_ACCENT_REPLACE = 12;

// DLL hooking modes set in the mapped memory, the uiMode should be one of these values
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

constexpr size_t KEY_BUF_LEN = 10;

constexpr wchar_t payloadNameW[] = L"consoleidator-injectable.dll";
constexpr char payloadNameA[] = "consoleidator-injectable.dll";

// verification constant for WM_COPYDATA
constexpr DWORD WM_COPYDATA_VERIFICATION = 0x4D4F4F43;

struct Keycombo
{
	SHORT bitmask{};
    DWORD vkCode{};
	WPARAM message{};
};

// self defined modifiers for hotkeys
// TODO: port to Windows.h defined modifier macros
constexpr int SHIFT_MOD = 0x1;
constexpr int CONTROL_MOD = 0x2;
constexpr int ALT_MOD = 0x4;

// 0x4D is the ASCII code for the 'M' key
// 0x54 is the ASCII code for the 'T' key
// 0x48 is the ASCII code for the 'H' key
constexpr DWORD VK_M = 0x4D;
constexpr DWORD VK_T = 0x54;
constexpr DWORD VK_H = 0x48;

// the message loop uses this table to look for a matching bitmask and key combination
constexpr Keycombo registeredCombos[] = {
	{CONTROL_MOD | SHIFT_MOD | ALT_MOD, VK_F5, HOTKEY_TOGGLE_SHOW},
	{CONTROL_MOD | SHIFT_MOD, VK_DELETE, HOTKEY_CLEAR_CONSOLE},
	{CONTROL_MOD | SHIFT_MOD, VK_M, HOTKEY_MAXIMIZE_BUFFER},
	{CONTROL_MOD, VK_HOME, HOTKEY_RESET_INVERT},
	{CONTROL_MOD, VK_END, HOTKEY_RESET},
	{CONTROL_MOD, VK_RIGHT, HOTKEY_FOREGROUND_FORWARD},
	{CONTROL_MOD, VK_LEFT, HOTKEY_FOREGROUND_BACKWARD},
	{CONTROL_MOD, VK_UP, HOTKEY_BACKGROUND_FORWARD},
	{CONTROL_MOD, VK_DOWN, HOTKEY_BACKGROUND_BACKWARD},
	{CONTROL_MOD | SHIFT_MOD, VK_T, HOTKEY_SET_TITLE},
	{CONTROL_MOD | SHIFT_MOD, VK_H, HOTKEY_HELP},
	{CONTROL_MOD, VK_SPACE, HOTKEY_ACCENT_REPLACE}
};

struct KeyDescriptor
{
	DWORD vkCode{};
	DWORD scanCode{};
	DWORD time{};
	bool controlDown = false;
	bool shiftDown = false;
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
};
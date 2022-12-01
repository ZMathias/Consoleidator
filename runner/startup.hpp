#pragma once
#include <Windows.h>
#include "resource.h"
#include "logger.hpp"

BOOL AddToStartup();
BOOL RemoveFromStartup();
void ToggleStartup(HWND hWnd);
void SetStartupCheckboxGreyed(HWND hWnd, bool state);
void SetStartupCheckmark(HWND hWnd, bool state);
BOOL IsStartup();
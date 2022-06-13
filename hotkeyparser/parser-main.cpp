#include "pch.h"
#include <fstream>
#include <string>
#include "parser-constants.hpp"

HWND* parenthWndPointer;
HWND parenthWnd;

extern "C" __declspec(dllexport) LRESULT KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
        const auto key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (wParam == WM_KEYDOWN)
        {
	        for (int i = 0; i < sizeof registeredKeys / sizeof DWORD; ++i)
	        {
                if (registeredKeys[i] == key->vkCode)
                {
	                SendMessage(parenthWnd, WM_PROCESS_KEY, key->vkCode, 0);
                }
	        }
        }
	}
    return CallNextHookEx(nullptr, code, wParam, lParam);
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
		    HANDLE hMapFile = OpenFileMappingA(
			   FILE_MAP_READ,
			   FALSE,
			   "Local\\HotKeyParser"
		   );

    		if (hMapFile == nullptr)
    		{
    			MessageBoxA(nullptr, "Could not open file mapping object in dll", "Error", MB_OK);
    			return FALSE;
    		}

    		parenthWndPointer = (HWND*)MapViewOfFile(
				hMapFile,
				FILE_MAP_READ,
				0,
				0,
				sizeof(HWND)
			);

    		if (parenthWndPointer == nullptr)
    		{
    			MessageBoxA(nullptr, ("Could not map view of file in dll with error" + std::to_string(GetLastError())).c_str(), "Error", MB_OK);
    			CloseHandle(hMapFile);
    			return FALSE;
    		}

            parenthWnd = *parenthWndPointer;
            CloseHandle(hMapFile);
            UnmapViewOfFile(parenthWndPointer);

            

    		break;
	    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


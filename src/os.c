#include <windows.h>

#include "macros.h"
#include "os.h"

LPWSTR GetLastErrorMessage() {
	LPWSTR pErrorStr = NULL;
	DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM;
	if(FormatMessage(flags, NULL, GetLastError(), 0, (LPWSTR)&pErrorStr, 0, NULL)) {
		return pErrorStr;
	}
	return L"Unknown error";
}

typedef BOOL (WINAPI *PSET_PROCESS_DPI_AWARENESS_CONTEXT)(DPI_AWARENESS_CONTEXT);

void EnableDPIAwareness() {
	static PSET_PROCESS_DPI_AWARENESS_CONTEXT fnSetProcessDPIAwarenessContext = NULL;

	// If the SetProcessDpiAwarenessContext function is not already loaded, attempt to load it
	if(!fnSetProcessDPIAwarenessContext) {
		// Make sure user32 is loaded
		LoadLibrary(L"user32");
		HMODULE hUser32 = GetModuleHandle(L"user32");
		fnSetProcessDPIAwarenessContext = (PSET_PROCESS_DPI_AWARENESS_CONTEXT)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
	}

	if(fnSetProcessDPIAwarenessContext) {
		DebugOutputF(L"Using SetProcessDpiAwarenessContext to set DPI scaling mode");
		if(!fnSetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
			DebugOutputF(L"DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 not supported");
			// If DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is not supported, fall back to the old API
			SetProcessDPIAware();
			DebugOutputF(L"Using SetProcessDPIAware to set DPI scaling mode");
		}
	}
	// If getting the function address failed, fall back to the old API
	else {
		SetProcessDPIAware();
		DebugOutputF(L"Using SetProcessDPIAware to set DPI scaling mode");
	}
}

#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>

#include "macros.h"

#include "loader.h"
#include "os.h"

#include "cmdline.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	// Dialog boxes should be scaled to system DPI
	EnableDPIAwareness();

	// Resolve the directory of this executable; WoW.exe and lichloader.txt are both
	// expected to sit next to it
	LPWSTR pWowDirectory = malloc(MAX_PATH * sizeof(WCHAR));
	GetModuleFileName(NULL, pWowDirectory, MAX_PATH);
	AssertMessageBoxF(GetLastError() != ERROR_INSUFFICIENT_BUFFER, L"GetLastError() == ERROR_INSUFFICIENT_BUFFER");
	PathRemoveFileSpec(pWowDirectory);

	LPWSTR pWowExePath = malloc(MAX_PATH * sizeof(WCHAR));
	PathCombine(pWowExePath, pWowDirectory, L"WoW.exe");

	WF_CMDLINE_PARSE_DATA cmdLineData = {0};
	cmdLineData.pWowExePath = pWowExePath;
	CmdLineParse(__argc, __wargv, &cmdLineData);

	LPWSTR pConfigPath = malloc(MAX_PATH * sizeof(WCHAR));
	PathCombine(pConfigPath, pWowDirectory, L"lichloader.txt");

	AssertMessageBoxF(GetFileAttributes(cmdLineData.pWowExePath) != INVALID_FILE_ATTRIBUTES,
		L"WoW.exe not found. Place LichLoader.exe in the same directory as WoW 3.3.5a.");

	WF_DLL_LIST_PARSE_DATA dllListData = {0};
	LoaderParseConfig(pWowDirectory, pConfigPath, &dllListData);

	STARTUPINFO startupInfo = {0};
	PROCESS_INFORMATION processInfo = {0};

	startupInfo.cb = sizeof(startupInfo);
	// Pass shortcut properties to WoW executable
	startupInfo.wShowWindow = nCmdShow;
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;

	LPWSTR pWowCmdLine = CmdLineFormat(&cmdLineData);

	// Create suspended so injected DLLs can hook the engine before any game code runs
	DWORD flags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;
	BOOL processCreated =
		CreateProcess(NULL, pWowCmdLine, NULL, NULL, FALSE, flags, NULL, NULL, &startupInfo, &processInfo);
	AssertMessageBoxF(processCreated,
		L"Error creating process: %ls\r\n"
		L"This issue can occur if you have enabled compatibility mode on the WoW executable.",
		GetLastErrorMessage());

	// Inject the LichCore coordinator first (a built-in, next to LichLoader.exe),
	// so its game-init hook is in place before consumer DllMains run. Its absence
	// is non-fatal: DLLs still load, but their Load() callbacks won't fire.
	int injectError = 0;
	LPWSTR pCorePath = malloc(MAX_PATH * sizeof(WCHAR));
	PathCombine(pCorePath, pWowDirectory, L"LichCore.dll");
	if(GetFileAttributes(pCorePath) != INVALID_FILE_ATTRIBUTES) {
		injectError = RemoteLoadLibrary(pCorePath, processInfo.hProcess);
	}
	else {
		MessageBoxF(L"LichCore.dll was not found next to LichLoader.exe.\r\n"
			L"DLLs will still load, but their Load() callbacks will not run.");
	}
	free(pCorePath);

	// Inject in the order listed in lichloader.txt; each DLL's DllMain runs before ResumeThread
	for(int i = 0; i < dllListData.nAdditionalDLLs; i++) {
		injectError = injectError || RemoteLoadLibrary(dllListData.pAdditionalDLLs[i], processInfo.hProcess);
	}

	if(injectError) {
		TerminateProcess(processInfo.hProcess, 0);
		return injectError;
	}

	ResumeThread(processInfo.hThread);

	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	return 0;
}

#pragma once

#include <windows.h>

typedef struct {
	// Arguments that should be passed to WoW
	LPWSTR *pWowArgs;
	// Number of arguments in pWowArgs
	int nWowArgs;
	// The game executable path
	LPWSTR pWowExePath;
} WF_CMDLINE_PARSE_DATA, *PWF_CMDLINE_PARSE_DATA;

// Parse the process command line, separating the WoW exe (if overridden) from
// the remaining pass-through arguments
void CmdLineParse(int argc, WCHAR **argv, PWF_CMDLINE_PARSE_DATA pOutput);

// Format the game command line: "WoW.exe" arg1 arg2 ...
LPWSTR CmdLineFormat(PWF_CMDLINE_PARSE_DATA pInput);

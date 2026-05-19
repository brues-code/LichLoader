#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>

#include "cmdline.h"

static void AppendArg(LPWSTR pArg, PWF_CMDLINE_PARSE_DATA pOutput) {
	int argsBufSize = (pOutput->nWowArgs + 1) * sizeof(LPWSTR);
	pOutput->pWowArgs = realloc(pOutput->pWowArgs, argsBufSize);
	pOutput->nWowArgs++;

	// The size of this buffer must be set to MAX_PATH
	LPWSTR pEscapedArg = malloc(MAX_PATH * sizeof(WCHAR));
	wcscpy(pEscapedArg, pArg);
	PathQuoteSpaces(pEscapedArg);

	pOutput->pWowArgs[pOutput->nWowArgs - 1] = pEscapedArg;
}

static BOOL IsWowExe(LPWSTR pArg) {
	return !!StrStrI(pArg, L".exe");
}

static BOOL IsLauncherExe(LPWSTR pArg) {
	return !!StrStrI(pArg, L"LichLoader.exe");
}

void CmdLineParse(int argc, WCHAR **argv, PWF_CMDLINE_PARSE_DATA pOutput) {
	for(int i = 1; i < argc; ++i) {
		// Is this argument an executable file?
		if(IsWowExe(argv[i])) {
			// Don't let our own path be reused as the game exe
			if(!IsLauncherExe(argv[i])) {
				// Otherwise allow overriding the default WoW.exe path
				pOutput->pWowExePath = argv[i];
			}
		}
		else {
			// If the argument is not an executable, pass it on to WoW
			AppendArg(argv[i], pOutput);
		}
	}
}

LPWSTR CmdLineFormat(PWF_CMDLINE_PARSE_DATA pInput) {
	LPWSTR pCmdLine = calloc(MAX_PATH, sizeof(WCHAR));

	// First, copy the name of the game executable into the command line
	wcscpy(pCmdLine, pInput->pWowExePath);
	PathQuoteSpaces(pCmdLine);

	// Calculate the characters (including NUL) required to hold all arguments
	int requiredChars = wcslen(pCmdLine) + 1;
	for(int i = 0; i < pInput->nWowArgs; ++i) {
		requiredChars += 1 + wcslen(pInput->pWowArgs[i]);
	}

	if(requiredChars > MAX_PATH) {
		pCmdLine = realloc(pCmdLine, requiredChars * sizeof(WCHAR));
	}

	// Copy the arguments into the command line
	LPWSTR pCurrent = pCmdLine + wcslen(pCmdLine);
	for(int i = 0; i < pInput->nWowArgs; ++i) {
		*(pCurrent++) = L' ';
		wcscpy(pCurrent, pInput->pWowArgs[i]);
		pCurrent += wcslen(pInput->pWowArgs[i]);
	}

	// End with NUL
	*pCurrent = L'\0';

	return pCmdLine;
}

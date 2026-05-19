#pragma once

#include <windows.h>

typedef struct {
	// Additional DLLs that the launcher should load
	LPWSTR *pAdditionalDLLs;
	// Number of arguments in pAdditionalDLLs
	int nAdditionalDLLs;
} WF_DLL_LIST_PARSE_DATA, *PWF_DLL_LIST_PARSE_DATA;

// Parse lichloader.txt into a list, excluding any DLLs that are not found in the game directory
void LoaderParseConfig(LPCWSTR pModulePath, LPCWSTR pConfigPath, PWF_DLL_LIST_PARSE_DATA pOutput);

// Inject a DLL into the target process via CreateRemoteThread(LoadLibraryW)
int RemoteLoadLibrary(LPWSTR pDllPath, HANDLE hTargetProcess);

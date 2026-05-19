#include <windows.h>
#include <shlwapi.h>

#include "macros.h"

#include "loader.h"
#include "textfile.h"

static void AppendArg(LPWSTR pArg, PWF_DLL_LIST_PARSE_DATA pOutput) {
	int argsBufSize = (pOutput->nAdditionalDLLs + 1) * sizeof(LPWSTR);
	pOutput->pAdditionalDLLs = realloc(pOutput->pAdditionalDLLs, argsBufSize);
	pOutput->pAdditionalDLLs[pOutput->nAdditionalDLLs++] = pArg;
}

// Normalize a path relative to pBase
static LPWSTR NormalizePath(LPCWSTR pBase, LPCWSTR pPath) {
	LPWSTR pAbsolutePath = calloc(MAX_PATH, sizeof(WCHAR));
	// If the path is relative, assume the file exists in pBase (and NOT the current directory)
	if(PathIsRelative(pPath)) {
		PathCombine(pAbsolutePath, pBase, pPath);
	}
	else {
		wcscpy(pAbsolutePath, pPath);
	}

	// Normalize path separators
	for(int i = 0; i < MAX_PATH && pAbsolutePath[i]; i++) {
		if(pAbsolutePath[i] == L'/') {
			pAbsolutePath[i] = L'\\';
		}
	}

	// Remove navigation elements such as "." and ".."
	LPWSTR pCanonicalizedPath = calloc(MAX_PATH, sizeof(WCHAR));
	PathCanonicalize(pCanonicalizedPath, pAbsolutePath);
	free(pAbsolutePath);

	return pCanonicalizedPath;
}

void LoaderParseConfig(LPCWSTR pModulePath, LPCWSTR pConfigPath, PWF_DLL_LIST_PARSE_DATA pOutput) {
	// No config file is fine — there's just nothing to inject
	if(GetFileAttributes(pConfigPath) == INVALID_FILE_ATTRIBUTES) {
		return;
	}

	int nLines = 0;
	LPWSTR* pLines = FromTextFile(pConfigPath, &nLines, TRUE);
	if(!pLines) {
		return;
	}

	for(int i = 0; i < nLines; i++) {
		LPWSTR pNormalizedPath = NormalizePath(pModulePath, pLines[i]);
		free(pLines[i]);

		DWORD attribs = GetFileAttributes(pNormalizedPath);
		// Skip silently if the path does not resolve to a real file — this lets
		// users keep entries for optional/disabled mods in lichloader.txt
		if(attribs == INVALID_FILE_ATTRIBUTES || attribs & FILE_ATTRIBUTE_DIRECTORY) {
			free(pNormalizedPath);
		}
		else {
			AppendArg(pNormalizedPath, pOutput);
		}
	}

	free(pLines);
}

int RemoteLoadLibrary(LPWSTR pDllPath, HANDLE hTargetProcess) {
	// Allocate memory for DLL path
	int dllPathLen = (wcslen(pDllPath) + 1) * sizeof(WCHAR);
	LPVOID pRemoteDllPath = VirtualAllocEx(
		hTargetProcess,
		NULL,
		dllPathLen,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE
	);

	// Copy the DLL path to the target process
	BOOL success = WriteProcessMemory(hTargetProcess, pRemoteDllPath, pDllPath, dllPathLen, NULL);
	AssertMessageBoxF(success, L"Failed to write process memory (pDllPath)");

	// LoadLibraryW shares its address between launcher and target because kernel32
	// loads at the same base in both 32-bit processes
	HANDLE hThread =
		CreateRemoteThread(hTargetProcess, NULL, 0, (LPTHREAD_START_ROUTINE)&LoadLibraryW, pRemoteDllPath, 0, NULL);
	AssertMessageBoxF(hThread, L"Failed to create remote thread");

	// Wait for the created thread to terminate
	WaitForSingleObject(hThread, 10000);

	DWORD threadResult = 0;
	// Exit code of the remote thread is LoadLibraryW's return (HMODULE truncated to DWORD)
	GetExitCodeThread(hThread, &threadResult);

	CloseHandle(hThread);

	AssertMessageBoxF(threadResult && threadResult != STILL_ACTIVE,
		L"DLL entry point returned an error loading:\r\n%ls\r\n\r\n"
		L"Make sure the DLL is built for Win32 (x86) and targets WoW 3.3.5a.",
		pDllPath
	);

	AssertMessageBoxF(VirtualFreeEx(hTargetProcess, pRemoteDllPath, 0, MEM_RELEASE),
		L"Failed to clean up process memory (pDllPath)");

	return 0;
}

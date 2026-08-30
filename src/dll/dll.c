#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>

#include <MinHook.h>

#include "macros.h"
#include "loader.h"

// CGlueMgr::Initialize in WoW 3.3.5a build 12340 (image base 0x400000).
//
// This is the login-screen UI init: it loads Interface\GlueXML\GlueXML.toc and
// runs exactly once, on the main thread, after the engine core is up but before
// the world's FrameXML script registration. We do not care what it does — we
// hook it solely to borrow a safe, post-boot, main-thread moment (outside the
// loader lock) at which to dispatch each consumer DLL's Load().
//
// The address is build-specific. Re-derive it for any client other than 12340.
#define GLUE_MGR_INITIALIZE_ADDR 0x004DA5F0

typedef void(__cdecl *GlueMgrInitialize_t)(void);
static GlueMgrInitialize_t o_GlueMgrInitialize = NULL;

// Contract for a consumer DLL's optional post-boot init hook. Nonzero = failure.
typedef DWORD(__cdecl *PLOAD)(void);

// Call each consumer DLL's exported Load() once, in lichloader.txt order. The
// launcher already injected them (via LoadLibraryW), so each is expected to be
// present in the process; we just resolve and invoke the callback.
static void DispatchConsumerLoads(void) {
	LPWSTR pGameDir = calloc(MAX_PATH, sizeof(WCHAR));
	GetModuleFileName(NULL, pGameDir, MAX_PATH);
	PathRemoveFileSpec(pGameDir);

	LPWSTR pConfigPath = calloc(MAX_PATH, sizeof(WCHAR));
	PathCombine(pConfigPath, pGameDir, L"lichloader.txt");

	// Re-read the same list the launcher injected from. LichCore.dll is not in
	// this list (the launcher injects it as a built-in), so we never dispatch to
	// ourselves.
	WF_DLL_LIST_PARSE_DATA dllList = {0};
	LoaderParseConfig(pGameDir, pConfigPath, &dllList);

	for(int i = 0; i < dllList.nAdditionalDLLs; i++) {
		HMODULE hModule = GetModuleHandle(dllList.pAdditionalDLLs[i]);
		if(!hModule) {
			// No handle means the launcher's injection failed earlier (already
			// reported). Nothing to call.
			DebugOutputF(L"No module handle for %ls; skipping Load()", dllList.pAdditionalDLLs[i]);
			free(dllList.pAdditionalDLLs[i]);
			continue;
		}

		PLOAD pLoad = (PLOAD)GetProcAddress(hModule, "Load");
		// Load() is optional — a DLL that installs its hooks from DllMain need not
		// export it.
		if(pLoad) {
			DWORD result = pLoad();
			DebugOutputF(L"Load() returned %lu for %ls", result, dllList.pAdditionalDLLs[i]);
			if(result) {
				MessageBoxF(L"A DLL reported a load failure (Load() returned %lu):\r\n%ls",
					result, dllList.pAdditionalDLLs[i]);
			}
		}

		free(dllList.pAdditionalDLLs[i]);
	}

	free(dllList.pAdditionalDLLs);
	free(pConfigPath);
	free(pGameDir);
}

// Detour for CGlueMgr::Initialize. Runs on the main thread as the login UI comes
// up. Let the game finish its own init first, then dispatch consumers exactly
// once (this function re-fires when returning to the login screen).
static void __cdecl h_GlueMgrInitialize(void) {
	o_GlueMgrInitialize();

	static BOOL dispatched = FALSE;
	if(!dispatched) {
		dispatched = TRUE;
		DispatchConsumerLoads();
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if(fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hinstDLL);

		// The process is still suspended here, so installing the hook now
		// guarantees it is active before any game code (and before consumer
		// DllMains that install their own hooks) runs.
		if(MH_Initialize() != MH_OK) {
			MessageBoxF(L"LichCore: MH_Initialize failed. Consumer Load() callbacks will not run.");
			return TRUE;
		}

		if(MH_CreateHook((LPVOID)GLUE_MGR_INITIALIZE_ADDR, &h_GlueMgrInitialize,
			   (LPVOID *)&o_GlueMgrInitialize) != MH_OK ||
			MH_EnableHook((LPVOID)GLUE_MGR_INITIALIZE_ADDR) != MH_OK) {
			MessageBoxF(L"LichCore: failed to hook game init. Consumer Load() callbacks will not run.");
			return TRUE;
		}
	}

	return TRUE;
}

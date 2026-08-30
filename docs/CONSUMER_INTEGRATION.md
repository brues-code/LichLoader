# Writing a DLL that LichLoader loads

Instructions for a consumer DLL (e.g. the Wrath-targeted ClassicAPI port) that
is injected by **LichLoader** and coordinated by **LichCore.dll**. Read this
before wiring up your DLL's entry points.

## TL;DR

- Build **Win32 (x86)**. A 64-bit DLL fails to load with no useful error.
- Install your engine hooks (MinHook etc.) from **`DllMain(DLL_PROCESS_ATTACH)`** — the process is suspended there, before any game code. This is the same thing 1.12.1 ClassicAPI does; it does **not** require anything from LichLoader.
- Optionally export **`Load()`** for post-boot, outside-the-loader-lock init. LichCore calls it once on the main thread after the engine is up.
- List your DLL in **`lichloader.txt`** (one path per line). Do **not** list `LichCore.dll` — the launcher injects it as a built-in.

## How your DLL gets loaded (lifecycle)

1. `LichLoader.exe` starts `WoW.exe` **suspended** and injects, in order, via `CreateRemoteThread(LoadLibraryW)`:
   1. `LichCore.dll` (built-in, always first)
   2. each DLL in `lichloader.txt`, top to bottom
   Injections are serialized — each DLL's `DllMain(DLL_PROCESS_ATTACH)` runs to completion before the next injection.
2. Every `DllMain` runs **while the process is still suspended**, before any game code, **under the Windows loader lock**.
3. `LichCore.dll`'s `DllMain` installs a MinHook hook on `CGlueMgr::Initialize` (`0x004DA5F0`, build 12340).
4. The launcher calls `ResumeThread`. The game boots.
5. When the login screen initializes, `CGlueMgr::Initialize` runs on the **main thread**; LichCore's detour calls the original, then — **once** — walks `lichloader.txt` and calls each DLL's exported `Load()` in list order.

So your DLL has **two** distinct initialization windows with very different rules.

## The two init windows

### Window A — `DllMain(DLL_PROCESS_ATTACH)`

- **When:** process suspended, before any game code, under the loader lock.
- **Use for:** `MH_Initialize`, creating/enabling your engine hooks. Because the game hasn't run yet, a hook you install here is guaranteed active before the engine calls the hooked function — including boot-time functions.
- **Must not:** call `LoadLibrary`, spawn threads that touch other modules, or otherwise do work that is unsafe under the loader lock. Read engine globals here and you'll get uninitialized memory.
- **Return `TRUE`.** Returning `FALSE` makes the injection report as a failure and the launcher terminates the game. Signal recoverable problems through `Load()` instead (see below).

This is where the bulk of a ClassicAPI-style DLL's work belongs. In particular, **any hook on a one-time boot function must be installed here**, not in `Load()` — see the timing note below.

### Window B — `Load()` (optional)

- **When:** main thread, after `CGlueMgr::Initialize` (login screen), **once**, **outside the loader lock**. All sibling DLLs are already mapped by this point.
- **Use for:** exactly the things `DllMain` can't do safely — `LoadLibrary` of helper modules, spawning worker threads, large/complex allocation, calling into sibling consumer DLLs, and one-time setup that needs the engine alive and engine globals populated.
- **Best-effort, not guaranteed.** `Load()` only fires if LichCore hooked the game successfully. On a non-12340 client, or if the hook fails, LichCore shows a warning and `Load()` never runs. **Do not put correctness-critical engine hooks behind `Load()`** — put those in `DllMain`, which always runs.

## The `Load()` export contract

LichCore resolves your callback with `GetProcAddress(hModule, "Load")` and calls
it as `DWORD __cdecl Load(void)`. Export it **undecorated**:

```cpp
// Load.cpp — exported callback
extern "C" __declspec(dllexport) unsigned long __cdecl Load(void) {
    // Post-boot init here. Runs on the main thread, outside the loader lock.
    // Return 0 on success; nonzero to report a failure.
    return 0;
}
```

- Return **0** for success. A **nonzero** return makes LichCore pop a MessageBox naming your DLL and the returned code — a user-visible, **non-fatal** error channel (the game keeps running).
- The export name must be exactly `Load` (no `_Load@0`, no mangling). `extern "C"` + `__cdecl` gives that under MSVC; if you use a `.def` file, list `Load` there.
- `Load()` is **optional**. A DLL that finishes everything in `DllMain` simply doesn't export it and LichCore skips it.

### Minimal skeleton (both windows)

```cpp
#include <windows.h>
#include <MinHook.h>

static bool InstallHooks() {
    if (MH_Initialize() != MH_OK) return false;
    // MH_CreateHook(...) / MH_EnableHook(...) for your engine hooks.
    return true;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        InstallHooks();          // hooks in place before the game runs
    }
    return TRUE;                 // never FALSE unless you mean to abort the launch
}

extern "C" __declspec(dllexport) unsigned long __cdecl Load(void) {
    // Heavy / loader-lock-unsafe / engine-dependent init.
    return 0;
}
```

## Timing note (important for the ClassicAPI port)

LichCore dispatches `Load()` from **`CGlueMgr::Initialize`**, which runs *after*
the glue (login) Lua state and its `LoadGlueScriptFunctions` registration, but
*before* the in-world `LoadScriptFunctions`.

Consequence for a ClassicAPI-style DLL that registers custom Lua C functions:

- Register by **hooking the FrameScript boot functions from `DllMain`** — the
  same pattern as 1.12.1 ClassicAPI (`FrameScript_Initialize` /
  `LoadScriptFunctions` / `LoadGlueScriptFunctions` created and enabled in
  `DllMain`, custom functions added from the post-hook). This does not use
  `Load()` at all.
- Do **not** try to install those boot-function hooks from `Load()`: the glue
  registration has already happened by the time `Load()` fires, so you'd miss
  it.

Use `Load()` for the *non-hook* side of init — config loading, background
threads, cross-DLL handshakes — that benefits from being outside the loader lock
and after boot.

## Build requirements

- **Win32 (x86)** only. Match LichLoader's non-negotiable bitness.
- Statically linking the CRT (`/MT`, `MSVC_RUNTIME_LIBRARY MultiThreaded`) is
  recommended, as LichLoader does, to avoid runtime-DLL load issues in the game
  process.
- No dependency on LichLoader headers is required — the only integration surface
  is the `Load()` export name and signature above.

## Deployment

- Put your built DLL somewhere under the game directory and add its path to
  `lichloader.txt` (UTF-8, one path per line, `#` for comments). Relative paths
  resolve against the game directory; absolute paths are honored as-is.
  Nonexistent paths are silently skipped.
- **Order in `lichloader.txt` is the `Load()` dispatch order.** If your DLL must
  initialize before/after another consumer, order the lines accordingly.
- `LichCore.dll` and `LichLoader.exe` sit next to `WoW.exe`. Do not list
  `LichCore.dll` in `lichloader.txt`.

## Verifying it works

- Put `OutputDebugStringW(L"...")` (or a MessageBox) at the top of `Load()` and
  watch with DebugView. It should fire once as the login screen appears.
- LichCore emits `OutputDebugString` traces prefixed `LichLoader:` for each DLL
  it dispatches (`Load() returned N for <path>`), and for any DLL it can't find
  a module handle for.
- If `Load()` never fires but the DLL clearly loaded (your `DllMain` ran), the
  client is probably not build 12340 and LichCore's hook address needs
  re-deriving — that's a LichLoader-side fix, not a consumer-side one.

## Failure modes / gotchas

| Symptom | Cause | Fix |
|---|---|---|
| DLL "load failed", game terminates | `DllMain` returned `FALSE`, or DLL is x64 | Return `TRUE`; build Win32 |
| `Load()` never called | No `Load` export / name mangled / wrong client build | `extern "C" __cdecl`; confirm build 12340 |
| MessageBox "Load() returned N" | Your `Load()` returned nonzero | Intended error channel — fix the underlying failure |
| Crash at boot | Hook installed too late, or unsafe work in `DllMain` | Install boot hooks in `DllMain`; move heavy work to `Load()` |

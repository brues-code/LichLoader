# LichLoader

A minimal DLL-injection launcher for **World of Warcraft: Wrath of the Lich King (3.3.5a)**.

LichLoader starts `WoW.exe` in a suspended state, injects the DLLs listed in `lichloader.txt`, and resumes the game. It performs no hooking, memory scanning, or patching of its own — it exists purely as a load mechanism for separate community DLLs that do.

It ships as two files:

- **`LichLoader.exe`** — the launcher. Injects the DLLs and starts the game.
- **`LichCore.dll`** — a tiny coordinator, injected automatically before your DLLs. It gives each DLL an optional, ordered, post-boot initialization callback (`Load()`). DLLs that only install hooks from `DllMain` don't need it and are unaffected.

## How it works

**Launcher (`LichLoader.exe`), while the game is suspended:**

1. `CreateProcess(WoW.exe, ..., CREATE_SUSPENDED, ...)` — the game's main thread is created but not started.
2. Inject `LichCore.dll` first, then each path in `lichloader.txt`, in order:
   - Allocate memory in the target and copy the wide DLL path into it (`VirtualAllocEx` + `WriteProcessMemory`).
   - `CreateRemoteThread` with `lpStartAddress = &LoadLibraryW` and the remote path as its argument. This works because `kernel32.dll` is mapped at the same base in both processes.
   - Wait for the thread, read its exit code (= `LoadLibraryW`'s `HMODULE`). Zero means the load failed and the launcher aborts with a message box.
3. `ResumeThread` — by the time the game executes a single instruction, every injected DLL's `DllMain(DLL_PROCESS_ATTACH)` has already run, which is the window for installing hooks before the engine touches anything.

**Coordinator (`LichCore.dll`), after the game boots:**

4. Its `DllMain` places one hook on the login-screen UI init. When that runs — on the main thread, with the engine up, outside the loader lock — `LichCore` calls each listed DLL's exported `Load()` once, in `lichloader.txt` order. `Load()` is optional; DLLs that don't export it are skipped.

## Requirements

- Windows.
- A 32-bit 3.3.5a `WoW.exe` (build 12340 — the standard 3.3.5a client).
- DLLs you want to inject must also be **Win32 (x86)** — a load failure with no obvious reason is almost always an accidentally x64-built consumer DLL.

The launcher and `LichCore.dll` are built x86 for the same reason: `CreateRemoteThread` into a 32-bit target needs a 32-bit `LoadLibraryW` address, which a 64-bit process can't provide.

## Install

Download the latest `LichLoader-<version>.zip` from the [Releases](../../releases) page and extract **both** `LichLoader.exe` and `LichCore.dll` into your WoW folder, next to `WoW.exe`. Then create a `lichloader.txt` (see below).

## Runtime layout

```
WoW/
├── WoW.exe
├── LichLoader.exe
├── LichCore.dll
├── lichloader.txt
└── ...
```

`lichloader.txt` is plain UTF-8, one DLL path per line, `#` introduces a comment. Relative paths resolve against the WoW directory; absolute paths are honored as-is. Lines pointing at files that don't exist are silently skipped — that lets you keep entries for optional/disabled mods without editing the file every time. **Do not list `LichCore.dll`** — the launcher injects it automatically.

Example `lichloader.txt`:

```
# Community engine extensions
awesome_wotlk.dll

# Optional — only loaded if the file actually exists
mods/experimental.dll
```

Any CLI arguments passed to `LichLoader.exe` are forwarded verbatim to `WoW.exe`, so shortcuts that pass realmlist overrides or `-console` keep working.

## For DLL authors — the `Load()` callback

Installing engine hooks from `DllMain(DLL_PROCESS_ATTACH)` works as it always has: the process is suspended at that point, so your hooks are in place before any game code runs. Most DLLs need nothing more.

For work that `DllMain` **can't** safely do — `LoadLibrary`, spawning threads, reading engine globals, calling into sibling DLLs — export a `Load()` callback and `LichCore.dll` will call it once, on the main thread, after the engine is up and outside the loader lock, in `lichloader.txt` order:

```cpp
extern "C" __declspec(dllexport) unsigned long __cdecl Load(void) {
    // Post-boot init. Return 0 on success, nonzero to report a failure.
    return 0;
}
```

`Load()` dispatch requires the standard build-12340 client; on other builds your DLL still loads, but the callback won't fire. Put anything correctness-critical in `DllMain`. Full contract and lifecycle: [docs/CONSUMER_INTEGRATION.md](docs/CONSUMER_INTEGRATION.md).

## Compatibility with awesome_wotlk

[awesome_wotlk](https://github.com/FrostAtom/awesome_wotlk) is a popular set of engine extensions and Lua API additions for 3.3.5a, distributed as a single DLL. LichLoader is fully compatible with it — drop `AwesomeWotlkLib.dll` next to `WoW.exe` and reference it in `lichloader.txt`:

```
AwesomeWotlkLib.dll
```

That's all. It installs its hooks from `DllMain` while the main thread is still suspended (no `Load()` needed), so they're in place before the engine starts — exactly as if it were loaded by any other launcher.

## Building

Both targets are built with CMake and **must** be configured as Win32. MinHook (used by `LichCore.dll`) is a git submodule, so clone recursively:

```powershell
git clone --recursive https://github.com/brues-code/LichLoader.git
# already cloned without --recursive:
git submodule update --init --recursive
```

From the **x86** Visual Studio Native Tools command prompt:

```powershell
cmake -B build -A Win32
cmake --build build --config Release
```

Or with Ninja from the same x86 VS environment:

```powershell
cmake -B build -G Ninja
ninja -C build
```

The CMake configure step intentionally fails with `FATAL_ERROR` if `CMAKE_SIZEOF_VOID_P != 4`, so an accidental x64 configure will not produce broken binaries.

Build output is written to `bin/LichLoader.exe` and `bin/LichCore.dll`.

## License

See the upstream [VanillaFixes](https://github.com/hannesmann/vanillafixes) repository for the original loader sources this project is derived from.

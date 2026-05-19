# LichLoader

A minimal DLL-injection launcher for **World of Warcraft: Wrath of the Lich King (3.3.5a)**.

LichLoader does one thing: it starts `WoW.exe` in a suspended state, injects any DLLs listed in `lichloader.txt`, and resumes the game. It performs no hooking, memory scanning, or patching of its own — it exists purely as a load mechanism for separate community DLLs that do.

It is a stripped-down fork of the loader half of [VanillaFixes](https://github.com/hannesmann/vanillafixes), retargeted for 3.3.5a.

## How it works

1. `CreateProcess(WoW.exe, ..., CREATE_SUSPENDED, ...)` — the game's main thread is created but not started.
2. For each path in `lichloader.txt`:
   - Allocate memory in the target process and copy the wide DLL path into it (`VirtualAllocEx` + `WriteProcessMemory`).
   - `CreateRemoteThread` with `lpStartAddress = &LoadLibraryW` and the remote path as its argument. This works because `kernel32.dll` is mapped at the same base in both processes.
   - Wait for the thread, read its exit code (= `LoadLibraryW`'s `HMODULE`). Zero means the load failed and the launcher aborts with a message box.
3. `ResumeThread` — by the time the game executes a single instruction, every injected DLL's `DllMain(DLL_PROCESS_ATTACH)` has already run, which is the window for installing hooks before the engine touches anything.

## Requirements

- Windows.
- A 32-bit 3.3.5a `WoW.exe`.
- DLLs you want to inject must also be **Win32 (x86)** — a load failure with no obvious reason is almost always an accidentally x64-built consumer DLL.

The launcher itself must be built x86 for the same reason: `CreateRemoteThread` into a 32-bit target needs a 32-bit `LoadLibraryW` address, which a 64-bit process can't provide.

## Runtime layout

Drop `LichLoader.exe` next to `WoW.exe`, then create a `lichloader.txt` in the same directory:

```
WoW/
├── WoW.exe
├── LichLoader.exe
├── lichloader.txt
└── ...
```

`lichloader.txt` is plain UTF-8, one DLL path per line, `#` introduces a comment. Relative paths resolve against the WoW directory; absolute paths are honored as-is. Lines pointing at files that don't exist are silently skipped — that lets you keep entries for optional/disabled mods without editing the file every time.

Example `lichloader.txt`:

```
# Community engine extensions
awesome_wotlk.dll

# Optional — only loaded if the file actually exists
mods/experimental.dll
```

Any CLI arguments passed to `LichLoader.exe` are forwarded verbatim to `WoW.exe`, so shortcuts that pass realmlist overrides or `-console` keep working.

## Compatibility with awesome_wotlk

[awesome_wotlk](https://github.com/FrostAtom/awesome_wotlk) is a popular set of engine extensions and Lua API additions for 3.3.5a, distributed as a single DLL. LichLoader is fully compatible with it — drop `awesome_wotlk.dll` next to `WoW.exe` and reference it in `lichloader.txt`:

```
awesome_wotlk.dll
```

That's all. Because the DLL is loaded while the main thread is still suspended, awesome_wotlk's hooks are in place before the engine starts, exactly the same as if it were loaded by any other launcher.

## Building

The launcher is C99, built with CMake, and **must** be configured as Win32.

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

The CMake configure step intentionally fails with `FATAL_ERROR` if `CMAKE_SIZEOF_VOID_P != 4`, so an accidental x64 configure will not produce a broken binary.

The build output is written to `bin/LichLoader.exe`.

## License

See the upstream [VanillaFixes](https://github.com/hannesmann/vanillafixes) repository for the original loader sources this project is derived from.

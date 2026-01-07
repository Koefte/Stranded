# SDL2 C++ Starter (CMake)

Minimal SDL2 windowed app for Windows (MSVC or MinGW) using CMake. The sample opens a window, clears the background, and draws a simple rectangle until you close or press Escape.

## Prerequisites
- CMake >= 3.16
- C++17 compiler (MSVC or MinGW)
- SDL2 development package (via vcpkg manifest or manual install)

## SDL2 via vcpkg (recommended)
1. Install vcpkg if you do not already have it: https://github.com/microsoft/vcpkg
2. Enable manifest mode (this repo includes `vcpkg.json`). Configure with your triplet:
   - MSVC: `-DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows`
   - MinGW: `-DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic`
3. vcpkg will build/download SDL2 automatically when you configure the project.

Tip: If `VCPKG_ROOT` is not set, this repo's `build.ps1` will also look for vcpkg at `C:/vcpkg`. If neither is available, it falls back to the local `vcpkg_installed/x64-windows` prefix and sets `SDL2_DIR` automatically.

## SDL2 manual install (alternative)
- Download the SDL2 development package for Windows from https://libsdl.org.
- Point CMake to the SDL2 config package, e.g. `-DSDL2_DIR="C:/SDL2-2.30.2/lib/cmake/SDL2"` (adjust for your path/version).
- Ensure `SDL2.dll` is discoverable at runtime (copy next to the built executable or add its folder to `PATH`).

## Configure and build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```
(For single-config generators like Ninja, omit `--config`.)

Or use the helper script (PowerShell):
```powershell
./build.ps1          # configure, build, run (Release)
./build.ps1 -Debug   # configure, build, run (Debug)
./build.ps1 -ConfigOnly
./build.ps1 -BuildOnly
```

## Run
- MSVC multi-config: `build/Release/stranded.exe`
- Single-config: `build/stranded.exe`

If SDL2 was installed manually, ensure `SDL2.dll` is on `PATH` or beside the executable before running.

## Notes
- `CMakeLists.txt` expects the SDL2 CMake config package to be discoverable. With vcpkg + manifest, this is handled automatically.
- Tweak `SDL_SetRenderDrawColor` calls in `src/main.cpp` to adjust colors.

## Troubleshooting
- SDL2 not found (SDL2Config.cmake): ensure vcpkg toolchain is used. Either set `VCPKG_ROOT`, install at `C:/vcpkg`, or pass `-DSDL2_DIR=.../vcpkg_installed/x64-windows/share/sdl2`.
- MSVC cannot find `stdio.h`: ensure the "Desktop development with C++" workload and a Windows 10/11 SDK are installed in Visual Studio, and build from a Developer PowerShell/Command Prompt. As an alternative, configure with the Visual Studio generator:
  ```powershell
  cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
        -DVCPKG_TARGET_TRIPLET=x64-windows
  cmake --build build --config Release
  ```

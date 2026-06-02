# FallHook

FallHook is a Fallout 4 F4SE/CommonLibF4 plugin for source-free runtime translation and hook support.

Target plugin output:

```text
Data/F4SE/Plugins/FallHook.dll
```

## Runtime Support

- Fallout 4 1.10.163
- Fallout 4 1.11.191

Each runtime has isolated source and include trees under `src/<runtime>/` and `include/<runtime>/`.

## Build

Prerequisites:

- Visual Studio 2022
- CMake 3.21+
- vcpkg, with `VCPKG_ROOT` set
- CommonLibF4, with `lib/commonlib-shared` available in that checkout

By default, CMake expects CommonLibF4 at `../CommonLibF4`. Override it if needed:

```powershell
cmake --preset fo4 -DCOMMONLIBF4_DIR=<path-to-CommonLibF4>
cmake --build build --config Release --target FallHook
```

The default deploy copy goes to `build/deploy/F4SE/Plugins`.

## License

GPL-3.0. See `LICENSE`.

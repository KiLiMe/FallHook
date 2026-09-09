# FallHook

[简体中文](README.md) | English

FallHook is a Fallout 4 F4SE/CommonLibF4 plugin for source-free runtime translation and hook support.

It loads xTranslator XML translations at runtime and applies them to in-game text, without
requiring the original/plugin source strings to be shipped or rebuilt into a new plugin.

Target plugin output:

```text
Data/F4SE/Plugins/FallHook.dll
```

## Runtime Support

| Runtime | Source tree | Notes |
| --- | --- | --- |
| Fallout 4 1.10.163 | `src/110163/`, `include/110163/` | dedicated module |
| Fallout 4 1.11.191 | `src/111191/`, `include/111191/` | AE module |
| Fallout 4 1.11.240 | reused from 1.11.191 | covered by the AE module, no separate tree |

The two runtime trees under `src/<runtime>/` and `include/<runtime>/` own hook offsets and
runtime layout assumptions. FallHook resolves every hook through the Address Library
(`REL::ID`), so the AE (1.11.x) module written against 1.11.191 also runs on newer AE patches
such as 1.11.240 with no code changes — that is why there is no `src/111240/` directory.

## Install

1. Install [F4SE](https://f4se.silverlock.org/) for your Fallout 4 version.
2. Copy `FallHook.dll` and `FallHook.ini` into:

   ```text
   <Fallout 4>/Data/F4SE/Plugins/
   ```

3. Place xTranslator XML files in:

   ```text
   <Fallout 4>/Data/F4SE/Plugins/FallHook/
   ```

## Configuration

Settings live in `Data/F4SE/Plugins/FallHook.ini` (shipped as `config/FallHook.ini`).
Highlights:

| Section | Key | Default | Purpose |
| --- | --- | --- | --- |
| `[XML]` | `LoadOrderMode` | `plugin` | `plugin` follows active plugin order; `filename` uses global filename ascending order |
| `[InGameTextHook]` | `Enable` | `false` | in-game text hook |
| `[Watchdog]` | `Enable` | `false` | slow/stuck load and hook warnings |
| `[ActivityWatchdog]` | `Enable` | `false` | verbose per-hook activity logging for stutter attribution |
| `[Logging]` | `EnableDebugLog` | `false` | debug trace logging |

## Build

Prerequisites:

- Visual Studio 2022
- CMake 3.21+
- vcpkg, with `VCPKG_ROOT` set
- [CommonLibF4](https://github.com/libxse/commonlibf4), with `lib/commonlib-shared` available in that checkout

By default, CMake expects CommonLibF4 at `../CommonLibF4`. Override it if needed:

```powershell
cmake --preset fo4 -DCOMMONLIBF4_DIR=<path-to-CommonLibF4>
cmake --build build --config Release --target FallHook
```

The default deploy copy goes to `build/deploy/F4SE/Plugins`.

## Tests

```powershell
cmake --build build --config Release --target FallHookCoreTests
ctest --test-dir build -C Release --output-on-failure
```

## Continuous Integration

| Workflow | Trigger | Result |
| --- | --- | --- |
| `build` | push / PR to `main` | builds the plugin, runs unit tests, uploads the `FallHook` artifact |
| `release` | `v*` tag push | builds, tests, packages `FallHook-<tag>.zip`, publishes a GitHub Release |

## License

GPL-3.0. See `LICENSE` and `NOTICE.md`.

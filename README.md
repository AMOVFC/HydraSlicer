<div align="center">

<!-- TODO: Replace with HydraSlicer logo when available -->
<picture>
  <img alt="HydraSlicer logo" src="resources/images/OrcaSlicer.png" width="15%" height="15%">
</picture>

# HydraSlicer

**A feature fork of [OrcaSlicer](https://github.com/SoftFever/OrcaSlicer) built for Klipper power users and multi-printer workflows.**

[![GitHub Repo stars](https://img.shields.io/github/stars/amovfc/hydraslicer)](https://github.com/amovfc/hydraslicer/stargazers)
[![Build all](https://img.shields.io/github/actions/workflow/status/amovfc/hydraslicer/build_all.yml?branch=main&label=build)](https://github.com/amovfc/hydraslicer/actions/workflows/build_all.yml)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)

</div>

---

## Why HydraSlicer?

OrcaSlicer is an excellent general-purpose slicer, but Klipper users with multiple printers face real friction: manually keeping slicer profiles in sync with Moonraker, switching between printer configs when batch-slicing parts for different machines, and waiting for sequential plate slicing to finish one at a time.

HydraSlicer solves these problems by adding **first-class Klipper/Moonraker integration**, **multi-printer project workflows**, and **parallel plate slicing** on top of every feature OrcaSlicer already provides. It automatically syncs with upstream OrcaSlicer daily, so you never fall behind on upstream improvements.

---

## HydraSlicer Features

- **Klipper/Moonraker Profile Sync**
  Automatic bidirectional synchronization of printer, filament, and process profiles between the slicer and your Klipper printers via the Moonraker API. Profiles are pulled when a printer is activated and pushed on export or slice.

- **Multi-Printer Slicing**
  Assign each build plate in a project to a different physical printer, each with its own filament and process presets. Slice a Voron V0 plate and an Ender 5 plate in the same project file.

- **Parallel Multi-Plate Slicing**
  Slice multiple build plates concurrently using TBB worker threads instead of processing them one at a time. Significant time savings for multi-plate projects.

- **Cloud Preset Backup**
  Optionally back up your presets to a Git repository or Supabase-hosted storage for versioned, cross-machine synchronization.

- **OAuth Authentication**
  Sign in with GitHub or Google for cloud features via Supabase-based PKCE authentication.

> **All OrcaSlicer features included:** HydraSlicer inherits every OrcaSlicer feature — calibration tools, precise wall/seam control, sandwich mode, polyholes, adaptive bed mesh, network printer support (Klipper, PrusaLink, OctoPrint), 200+ built-in printer profiles, and more. See the [OrcaSlicer feature list](https://github.com/SoftFever/OrcaSlicer#main-features) for the full list.

---

## Download

### Stable Release

**[Download the Latest Stable Release](https://github.com/amovfc/hydraslicer/releases/latest)**

### Nightly Builds

**[Download the Latest Nightly Build](https://github.com/amovfc/hydraslicer/releases/tag/nightly-builds)**

---

## How to Install

### Windows

Download the **Windows Installer (.exe)** from the [releases page](https://github.com/amovfc/hydraslicer/releases).

A portable ZIP build is also available.

<details>
<summary>Troubleshooting</summary>

If you have trouble running the application, you may need to install the following runtimes:

- **Microsoft Edge WebView2 Runtime**
  - [Download from Microsoft](https://go.microsoft.com/fwlink/p/?LinkId=2124703)
  - [Details](https://aka.ms/webview2)
- **Visual C++ Redistributable 2019 (x64)**
  - [Download from Microsoft](https://aka.ms/vs/17/release/vc_redist.x64.exe)
  - This may already be installed if you have Visual Studio. Check: `%VCINSTALLDIR%Redist\MSVC\v142`

</details>

### macOS

*Coming soon.* In the meantime, see `build_release_macos.sh` for build instructions.

### Linux

*Coming soon.* In the meantime, see `build_linux.sh` for build instructions.

---

## How to Build (Windows)

### Prerequisites

| Requirement | Notes |
|---|---|
| **Visual Studio 2019 or 2022** | With the "Desktop development with C++" workload |
| **CMake 3.13+** | Must appear in PATH *before* Strawberry Perl's `c\bin` |
| **Strawberry Perl** | Required for building certain dependencies |
| **Git** | For cloning and submodule operations |
| **Python 3** | Used by localization and build scripts |

> **PATH ordering:** CMake (`C:\Program Files\CMake\bin`) must come before Strawberry Perl (`C:\Strawberry\c\bin`) in your system PATH. The build will fail with a clear error if this is not the case.

### Quick Method (Recommended)

Use the provided batch scripts from a **Developer Command Prompt** (or any terminal with `msbuild` on PATH):

**Visual Studio 2022:**
```batch
:: Full build — dependencies + slicer
build_release_vs2022.bat

:: Dependencies only
build_release_vs2022.bat deps

:: Slicer only (after dependencies are built)
build_release_vs2022.bat slicer
```

**Auto-detect Visual Studio version (2019/2022/2026):**
```batch
build_release_vs.bat
build_release_vs.bat deps
build_release_vs.bat slicer
```

Append `debug` or `debuginfo` for Debug or RelWithDebInfo builds:
```batch
build_release_vs2022.bat debug
build_release_vs2022.bat slicer debuginfo
```

### Manual CMake Steps

If you prefer full control over the build process:

```batch
:: 1. Build dependencies
cd deps
mkdir build
cd build
cmake ../ -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --target deps -- -m
cd ..\..

:: 2. Build the slicer
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DORCA_TOOLS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --target ALL_BUILD -- -m

:: 3. Build localization files
call scripts\run_gettext.bat

:: 4. Install (copies executables, DLLs, and resources into place)
cmake --build . --target install --config Release
```

Replace `"Visual Studio 17 2022"` with `"Visual Studio 16 2019"` if using VS2019.

### Creating the Windows Installer (.exe)

The project uses **NSIS** (Nullsoft Scriptable Install System) via CMake's CPack to generate the installer. After building and installing:

```batch
cd build
cmake --build . --target package --config Release
```

This produces an installer file named `OrcaSlicer_Windows_Installer_V{VERSION}.exe` (e.g., `OrcaSlicer_Windows_Installer_V2.3.2-dev.exe`). The installer:

- Installs all executables, DLLs, and resources
- Creates a desktop shortcut
- Registers an uninstaller
- Supports silent installation via NSIS flags

### Hydra Build Flag

HydraSlicer features are controlled by the CMake option `SLIC3R_ENABLE_HYDRA`, which defaults to **ON**.

To build a vanilla OrcaSlicer-equivalent binary without any Hydra features:
```batch
cmake .. -G "Visual Studio 17 2022" -A x64 -DSLIC3R_ENABLE_HYDRA=OFF -DCMAKE_BUILD_TYPE=Release
```

### macOS and Linux Build

Build instructions for macOS and Linux are pending. For now, refer to:
- **macOS:** `build_release_macos.sh` — supports arm64, x86_64, and universal builds
- **Linux:** `build_linux.sh` — supports Ubuntu/Debian with AppImage output

---

## Klipper Note

If you are running Klipper, it is recommended to add the following configuration to your `printer.cfg` file:

```gcode
# Enable object exclusion
[exclude_object]

# Enable arcs support
[gcode_arcs]
resolution: 0.1
```

---

## Upstream Sync

HydraSlicer automatically syncs with upstream OrcaSlicer daily via a GitHub Actions workflow. Clean merges produce a PR for review; conflicting merges produce a draft PR with conflict details.

All Hydra-specific code is guarded by `#ifdef SLIC3R_ENABLE_HYDRA` preprocessor blocks and lives primarily in `src/hydra/`, minimizing merge conflicts with upstream changes.

---

## Contributing

Contributions are welcome! Please open issues and pull requests on the [amovfc/hydraslicer](https://github.com/amovfc/hydraslicer) repository.

- Hydra-specific features **must** be gated behind `#ifdef SLIC3R_ENABLE_HYDRA`
- Bugs in core OrcaSlicer functionality should be reported to the [upstream OrcaSlicer project](https://github.com/SoftFever/OrcaSlicer)

---

## Lineage

HydraSlicer is a feature fork of [OrcaSlicer](https://github.com/SoftFever/OrcaSlicer) by SoftFever.

OrcaSlicer was originally forked from [Bambu Studio](https://github.com/bambulab/BambuStudio) by BambuLab, which is forked from [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which descends from [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community.

OrcaSlicer also incorporates features from [SuperSlicer](https://github.com/supermerill/SuperSlicer) by @supermerill.

HydraSlicer is maintained by [AMOVFC](https://github.com/amovfc).

---

## License

- **HydraSlicer** is licensed under the GNU Affero General Public License, version 3. HydraSlicer is based on OrcaSlicer by SoftFever.
- **OrcaSlicer** is licensed under the GNU Affero General Public License, version 3. OrcaSlicer is based on Bambu Studio by BambuLab.
- **Bambu Studio** is licensed under the GNU Affero General Public License, version 3. Bambu Studio is based on PrusaSlicer by PrusaResearch.
- **PrusaSlicer** is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.
- **Slic3r** is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.
- The **GNU Affero General Public License**, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.
- OrcaSlicer includes a **pressure advance calibration pattern test** adapted from Andrew Ellis' generator, which is licensed under GNU General Public License, version 3. Ellis' generator is itself adapted from a generator developed by Sineos for Marlin, which is licensed under GNU General Public License, version 3.
- The **Bambu networking plugin** is based on non-free libraries from BambuLab. It is optional and provides extended functionalities for Bambulab printer users.

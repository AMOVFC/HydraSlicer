# Hydra Integration README

This is the **second README** with practical instructions for running and validating the Hydra integration added under `src/hydra/`.

## What was added

Hydra is built as an isolated static library and linked into OrcaSlicer:

- `src/hydra/CMakeLists.txt` defines `add_library(hydra STATIC ...)`
- `src/CMakeLists.txt` adds `add_subdirectory(hydra)` and links `hydra` into:
  - `OrcaSlicer`
  - Windows launcher target `OrcaSlicer_app_gui`

## Build instructions

From repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target OrcaSlicer --config Release --parallel
```

If you only want to validate target wiring quickly, run just configure:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

## Hydra runtime behavior (high level)

Hydra is triggered from two hook points in GUI flows:

1. **Printer preset activation**
   - Calls `HydraSyncManager::Instance().OnPrinterActivated(printer_id)`
   - Expected behavior: pull baseline profile from Moonraker and cache/hash locally.

2. **Preset save / pre-export / pre-slice**
   - Calls `HydraSyncManager::Instance().OnPresetsPossiblyChanged(printer_id)`
   - Expected behavior: export current profile, compare hash to baseline, push versioned file if changed.

## How to configure in UI

1. Start OrcaSlicer and open a project.
2. In the main side tools area, click the Hydra status text (`Hydra: ...`).
3. In **Printer-resident Profiles** dialog, set:
   - Hydra printer ID
   - Moonraker base URL (example: `http://printer-host:7125`)
   - Auto-pull on printer activation
   - Auto-push on export/slice
   - Optional SD/USB fallback mount path
4. Click **Test Connection** to run a Moonraker list call for `config/slicer/<HYDRA_PRINTER_ID>`.
5. Click **Save**.

Hydra settings are persisted to:

- `~/.config/OrcaSlicer/hydra_settings.json` (platform path varies by Orca data dir)

Hydra cache path:

- `<Orca data dir>/hydra_cache/<HYDRA_PRINTER_ID>/...`


## Optional cloud backup options

Hydra now supports two optional cloud-save strategies per printer preset:

1. **Supabase + GitHub sign-in (configuration-only in this patch)**
   - Select `Supabase + GitHub sign-in` in the Hydra dialog.
   - Enter Supabase URL, anon key, and a user access token obtained from your auth flow.
   - Current implementation validates configuration presence before sync; app-side OAuth UI flow is not yet implemented.

2. **Git repository backup (fully wired)**
   - Select `Git repository backup`.
   - Enter a remote URL (for example `https://github.com/<user>/<repo>.git`), branch, and optional GitHub token.
   - Use **Test Cloud Backup** to run `git ls-remote` validation.
   - On preset push, Hydra writes version files + `meta.json`, commits, and pushes to the configured branch.

This makes Bambu integration unnecessary for users that prefer self-managed GitHub or generic Git storage.

## Manual test checklist

### A) Baseline pull on printer activation

1. Configure a printer + valid Moonraker URL.
2. Switch active printer preset in Orca.
3. Confirm status changes to success (`Hydra: in sync`) and baseline cache files appear:
   - `baseline.orca_printer`
   - `baseline.sha256`

### B) Push on preset changes

1. Modify printer/filament/print settings.
2. Save preset or use slice/export flow.
3. Confirm Hydra pushes a versioned profile to:

```text
config/slicer/<HYDRA_PRINTER_ID>/versions/<timestamp>.orca_printer
```

4. Confirm `meta.json` is updated with latest hash/timestamp.

### C) No-op when unchanged

1. Repeat save/export without changing presets.
2. Confirm no additional version upload occurs.

## Suggested command-line checks

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build app
cmake --build build --target OrcaSlicer --config Release --parallel

# Optional tests (if environment has deps)
cmake --build build --target tests
ctest --test-dir build --output-on-failure
```

## Troubleshooting

- Configure failure mentioning DBus usually means missing system development packages in the environment.
- If Test Connection fails:
  - Verify Moonraker URL and port
  - Ensure printer is reachable from host/container
  - Ensure Moonraker file APIs are enabled
- If status shows sync failed, hover status label for last error tooltip.

## Scope note

Hydra logic is intentionally isolated to `src/hydra/` with minimal hook calls in existing GUI code paths.

# Hydra Integration

Hydra enables printer-resident profile synchronization between OrcaSlicer and Klipper/Moonraker printers, with optional cloud backup via Git repositories or Supabase.

## Architecture

Hydra is built as an isolated static library (`src/hydra/`) linked into OrcaSlicer:

| Module | Purpose |
|---|---|
| `HydraSettings` | Per-printer-preset configuration (Moonraker URL, sync options, cloud backup) |
| `HydraSyncManager` | Orchestrates pull-on-activate and push-on-export sync flows |
| `HydraMoonrakerClient` | HTTP client for Moonraker file server API |
| `HydraLocalStore` | Local cache with atomic writes and SHA-256 content hashing |
| `HydraPresetBundle` | Pluggable import/export handlers bridging Hydra to OrcaSlicer presets |
| `HydraGitSync` | Git-based version control backup for preset bundles |
| `HydraPlatePlan` | Per-plate printer/filament/print preset assignments and manifest generation |

## Build

From repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target OrcaSlicer --config Release --parallel
```

## Runtime Behavior

### Pull on printer activation
When a printer preset is activated, `HydraSyncManager::OnPrinterActivated` downloads the baseline profile from Moonraker (`config/slicer/<HYDRA_PRINTER_ID>/profile.orca_printer`), hashes it, and caches it locally. If the hash differs from the cached baseline, the profile is imported.

### Push on export/slice
When presets change or G-code is exported, `HydraSyncManager::OnPresetsPossiblyChanged` exports the current profile, compares its hash to the baseline, and pushes a versioned copy to Moonraker and optionally to a Git repository.

## UI Configuration

1. Click the "Hydra: ..." status label in the side tools area.
2. Configure:
   - **Hydra printer ID** - identifier matching your Moonraker config directory
   - **Moonraker base URL** - e.g. `http://printer-host:7125`
   - **Auto-pull / Auto-push** toggles
   - **Cloud backup mode** - Disabled, Supabase+GitHub, or Git repository
3. Use **Test Connection** / **Test Cloud Backup** to validate.

Settings are persisted to `<data_dir>/hydra_settings.json`.

## Improvements over initial implementation

- HTTP status code checking in Moonraker client (not just curl success)
- Thread-safe status callback in SyncManager (mutex-protected)
- Exception handling in background sync threads
- Structured logging via Boost.Log instead of silent catch-all
- Atomic write verifies stream state before rename
- Download endpoint uses correct Moonraker file path API
- Generic HTTPS token injection (not GitHub-specific)

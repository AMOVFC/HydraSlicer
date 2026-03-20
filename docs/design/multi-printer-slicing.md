# Multi-Printer Slicing Design Document

## Overview

HydraSlicer's flagship feature: assign each build plate to a different printer
with its own filament and process presets. This enables workflows like slicing
high-temp parts for a Voron V0 (enclosed, filtered) while simultaneously
preparing large parts for an Ender 5 (open air), all within a single project.

## Architecture

### Current State

- All plates share a single global printer/filament/process preset (managed by `PresetBundle`)
- Each `PartPlate` already has a `DynamicPrintConfig m_config` for plate-local overrides
  (bed type, print sequence, spiral mode, filament maps)
- Slicing is per-plate: `update_slice_context()` sets up `BackgroundSlicingProcess` for one plate at a time
- `Print::apply(model, config)` takes a merged `DynamicPrintConfig` (full_config)

### Design: Per-Plate Preset Assignment

#### Data Model

Add `PlatePresetOverride` to `PartPlate` to store per-plate preset selections:

```cpp
struct PlatePresetOverride {
    std::string printer_preset_name;      // empty = use global
    std::string process_preset_name;      // empty = use global
    std::vector<std::string> filament_preset_names; // empty = use global

    bool has_printer_override() const { return !printer_preset_name.empty(); }
    bool has_process_override() const { return !process_preset_name.empty(); }
    bool has_filament_override() const { return !filament_preset_names.empty(); }
    bool has_any_override() const {
        return has_printer_override() || has_process_override() || has_filament_override();
    }
};
```

#### Config Resolution

When slicing a plate, build the full config by:
1. Start with global PresetBundle selections (current behavior)
2. If plate has printer override, substitute that printer's config
3. If plate has process override, substitute that process config
4. If plate has filament overrides, substitute those filament configs
5. Apply existing plate-local overrides (bed type, print seq, etc.)

New method `PartPlate::build_full_config()`:
```cpp
DynamicPrintConfig PartPlate::build_full_config(PresetBundle& bundle) const {
    if (!m_preset_override.has_any_override()) {
        // Use global config + plate overrides (existing behavior)
        DynamicPrintConfig config = bundle.full_config();
        config.apply(m_config, true);
        return config;
    }
    // Build config from overridden presets
    // ...
}
```

#### Bed Size Handling

Different printers have different bed sizes. When a plate has a printer override:
- The plate stores its own bed dimensions derived from the printer preset
- Visual rendering shows the correct bed size per plate
- Arrangement and collision detection use per-plate dimensions
- The plate grid layout accommodates the largest plate dimensions

#### Slicing Pipeline Changes

`PartPlate::update_slice_context()` is modified to:
1. Build the plate-specific full config
2. Apply it to the Print object via `Print::apply(model, plate_config)`

#### UI Changes

Extend `PlateSettingsDialog` with:
- Printer preset dropdown (shows all available printers)
- Process preset dropdown (filtered by compatibility with selected printer)
- Filament preset dropdowns (filtered by compatibility)
- "Use Global" option as default for each

Add visual indicator on plate tab showing assigned printer name.

#### Serialization

Per-plate preset overrides are stored in:
- The existing `m_config` DynamicPrintConfig (for 3MF compatibility)
- New config keys: `plate_printer_preset`, `plate_process_preset`, `plate_filament_presets`

## Implementation Phases

### Phase 1: Core Data Model (this PR)
- Add `PlatePresetOverride` struct
- Add preset override storage to `PartPlate`
- Add `build_full_config()` method
- Modify `update_slice_context()` to use per-plate config
- Serialization/deserialization support

### Phase 2: UI Integration
- Extend `PlateSettingsDialog` with printer/filament/process selection
- Add plate tab visual indicators
- Sidebar updates when switching plates with overrides

### Phase 3: Per-Plate Bed Size
- Dynamic bed size per plate based on assigned printer
- Plate grid layout adaptation
- Arrangement support for heterogeneous plate sizes

### Phase 4: Live Printer Integration
- Query connected printers for loaded filament/nozzle info
- Display printer status in plate assignment UI
- Auto-suggest printer assignment based on material requirements

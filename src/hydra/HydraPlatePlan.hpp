#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Slic3r::Hydra {

struct PlateAssignment {
    std::string hydra_printer_id;
    std::string printer_preset;
    std::string filament_preset;
    std::string print_preset;
};

struct PlateManifestEntry {
    std::string plate_uuid;
    std::string gcode_file;
    PlateAssignment assignment;
    std::string preset_hash;
};

class HydraPlatePlan {
public:
    void set_assignment(const std::string &plate_uuid, PlateAssignment assignment);
    const std::unordered_map<std::string, PlateAssignment> &assignments() const { return m_assignments; }

    std::string serialize_hydra_section() const;
    void        deserialize_hydra_section(const std::string &json_blob);

    std::string build_manifest_json(const std::vector<PlateManifestEntry> &entries) const;

private:
    std::unordered_map<std::string, PlateAssignment> m_assignments;
};

} // namespace Slic3r::Hydra

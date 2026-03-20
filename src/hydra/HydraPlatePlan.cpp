#include "HydraPlatePlan.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <sstream>

namespace Slic3r::Hydra {

using boost::property_tree::ptree;

void HydraPlatePlan::set_assignment(const std::string &plate_uuid, PlateAssignment assignment)
{
    m_assignments[plate_uuid] = std::move(assignment);
}

std::string HydraPlatePlan::serialize_hydra_section() const
{
    ptree root;
    for (const auto &kv : m_assignments) {
        ptree p;
        p.put("hydra_printer_id", kv.second.hydra_printer_id);
        p.put("printer_preset", kv.second.printer_preset);
        p.put("filament_preset", kv.second.filament_preset);
        p.put("print_preset", kv.second.print_preset);
        root.add_child("plate_assignments." + kv.first, p);
    }
    std::ostringstream ss;
    boost::property_tree::write_json(ss, root);
    return ss.str();
}

void HydraPlatePlan::deserialize_hydra_section(const std::string &json_blob)
{
    std::istringstream is(json_blob);
    ptree root;
    boost::property_tree::read_json(is, root);
    m_assignments.clear();
    for (const auto &kv : root.get_child("plate_assignments", ptree{})) {
        PlateAssignment pa;
        pa.hydra_printer_id = kv.second.get<std::string>("hydra_printer_id", "");
        pa.printer_preset   = kv.second.get<std::string>("printer_preset", "");
        pa.filament_preset  = kv.second.get<std::string>("filament_preset", "");
        pa.print_preset     = kv.second.get<std::string>("print_preset", "");
        m_assignments[kv.first] = pa;
    }
}

std::string HydraPlatePlan::build_manifest_json(const std::vector<PlateManifestEntry> &entries) const
{
    ptree root;
    ptree items;
    for (const auto &e : entries) {
        ptree item;
        item.put("plate_uuid", e.plate_uuid);
        item.put("gcode_file", e.gcode_file);
        item.put("preset_hash", e.preset_hash);
        item.put("hydra_printer_id", e.assignment.hydra_printer_id);
        item.put("printer_preset", e.assignment.printer_preset);
        item.put("filament_preset", e.assignment.filament_preset);
        item.put("print_preset", e.assignment.print_preset);
        items.push_back(std::make_pair("", item));
    }
    root.add_child("plates", items);
    std::ostringstream ss;
    boost::property_tree::write_json(ss, root);
    return ss.str();
}

} // namespace Slic3r::Hydra

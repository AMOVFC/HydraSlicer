#include "HydraPresetBundle.hpp"

namespace Slic3r::Hydra {

HydraPresetBundle &HydraPresetBundle::Instance()
{
    static HydraPresetBundle instance;
    return instance;
}

void HydraPresetBundle::set_export_handler(ExportBundleFn fn) { m_export = std::move(fn); }
void HydraPresetBundle::set_import_handler(ImportBundleFn fn) { m_import = std::move(fn); }

std::optional<std::string> HydraPresetBundle::export_bundle(const std::string &printer_preset_id) const
{
    if (!m_export) return std::nullopt;
    return m_export(printer_preset_id);
}

bool HydraPresetBundle::import_bundle(const std::string &bundle_payload, const std::string &printer_preset_id) const
{
    if (!m_import) return false;
    return m_import(bundle_payload, printer_preset_id);
}

} // namespace Slic3r::Hydra

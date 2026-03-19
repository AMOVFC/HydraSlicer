#pragma once

#include <functional>
#include <optional>
#include <string>

namespace Slic3r::Hydra {

class HydraPresetBundle {
public:
    using ExportBundleFn = std::function<std::optional<std::string>(const std::string &printer_preset_id)>;
    using ImportBundleFn = std::function<bool(const std::string &bundle_payload, const std::string &printer_preset_id)>;

    static HydraPresetBundle &Instance();

    void set_export_handler(ExportBundleFn fn);
    void set_import_handler(ImportBundleFn fn);

    std::optional<std::string> export_bundle(const std::string &printer_preset_id) const;
    bool                       import_bundle(const std::string &bundle_payload, const std::string &printer_preset_id) const;

private:
    ExportBundleFn m_export;
    ImportBundleFn m_import;
};

} // namespace Slic3r::Hydra

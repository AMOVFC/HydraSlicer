#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace Slic3r::Hydra {

struct PrinterSettings
{
    enum class CloudSyncMode { None, SupabaseGithub, GitRepository };

    std::string   hydra_printer_id;
    std::string   moonraker_base_url;
    bool          auto_pull_on_activate = true;
    bool          auto_push_on_export   = true;
    std::string   fallback_mount_path;
    CloudSyncMode cloud_sync_mode = CloudSyncMode::None;
    std::string   supabase_url;
    std::string   supabase_anon_key;
    std::string   supabase_access_token;
    std::string   git_remote_url;
    std::string   git_branch = "main";
    std::string   git_access_token;
};

class HydraSettings
{
public:
    static HydraSettings& Instance();

    void set_storage_path(const std::string& path);
    void load();
    void save() const;

    std::optional<PrinterSettings> get_for_preset(const std::string& preset_id) const;
    void                           set_for_preset(const std::string& preset_id, const PrinterSettings& settings);

private:
    mutable std::mutex                               m_mutex;
    std::string                                      m_storage_path;
    std::unordered_map<std::string, PrinterSettings> m_printers;
};

} // namespace Slic3r::Hydra

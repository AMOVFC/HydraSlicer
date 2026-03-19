#include "HydraSettings.hpp"

#include <boost/log/trivial.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace Slic3r::Hydra {

using boost::property_tree::ptree;

namespace {

using CloudSyncMode = Slic3r::Hydra::PrinterSettings::CloudSyncMode;

CloudSyncMode cloud_sync_mode_from_string(const std::string& value)
{
    if (value == "supabase_github")
        return CloudSyncMode::SupabaseGithub;
    if (value == "git_repository")
        return CloudSyncMode::GitRepository;
    return CloudSyncMode::None;
}

std::string cloud_sync_mode_to_string(CloudSyncMode mode)
{
    if (mode == CloudSyncMode::SupabaseGithub)
        return "supabase_github";
    if (mode == CloudSyncMode::GitRepository)
        return "git_repository";
    return "none";
}

} // namespace

HydraSettings& HydraSettings::Instance()
{
    static HydraSettings instance;
    return instance;
}

void HydraSettings::set_storage_path(const std::string& path)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_storage_path = path;
}

void HydraSettings::load()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_storage_path.empty())
        return;
    ptree root;
    try {
        boost::property_tree::read_json(m_storage_path, root);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(warning) << "Hydra: failed to load settings from " << m_storage_path << ": " << e.what();
        return;
    }
    m_printers.clear();
    for (const auto& item : root.get_child("printers", ptree{})) {
        PrinterSettings s;
        s.hydra_printer_id      = item.second.get<std::string>("hydra_printer_id", "");
        s.moonraker_base_url    = item.second.get<std::string>("moonraker_base_url", "");
        s.auto_pull_on_activate = item.second.get<bool>("auto_pull_on_activate", true);
        s.auto_push_on_export   = item.second.get<bool>("auto_push_on_export", true);
        s.fallback_mount_path   = item.second.get<std::string>("fallback_mount_path", "");
        s.cloud_sync_mode       = cloud_sync_mode_from_string(item.second.get<std::string>("cloud_sync_mode", "none"));
        s.supabase_url          = item.second.get<std::string>("supabase_url", "");
        s.supabase_anon_key     = item.second.get<std::string>("supabase_anon_key", "");
        s.supabase_access_token = item.second.get<std::string>("supabase_access_token", "");
        s.git_remote_url        = item.second.get<std::string>("git_remote_url", "");
        s.git_branch            = item.second.get<std::string>("git_branch", "main");
        s.git_access_token      = item.second.get<std::string>("git_access_token", "");
        m_printers[item.first]  = s;
    }
}

void HydraSettings::save() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_storage_path.empty())
        return;
    ptree root;
    ptree printers;
    for (const auto& kv : m_printers) {
        ptree entry;
        entry.put("hydra_printer_id", kv.second.hydra_printer_id);
        entry.put("moonraker_base_url", kv.second.moonraker_base_url);
        entry.put("auto_pull_on_activate", kv.second.auto_pull_on_activate);
        entry.put("auto_push_on_export", kv.second.auto_push_on_export);
        entry.put("fallback_mount_path", kv.second.fallback_mount_path);
        entry.put("cloud_sync_mode", cloud_sync_mode_to_string(kv.second.cloud_sync_mode));
        entry.put("supabase_url", kv.second.supabase_url);
        entry.put("supabase_anon_key", kv.second.supabase_anon_key);
        entry.put("supabase_access_token", kv.second.supabase_access_token);
        entry.put("git_remote_url", kv.second.git_remote_url);
        entry.put("git_branch", kv.second.git_branch);
        entry.put("git_access_token", kv.second.git_access_token);
        printers.add_child(kv.first, entry);
    }
    root.add_child("printers", printers);
    try {
        boost::property_tree::write_json(m_storage_path, root);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Hydra: failed to save settings to " << m_storage_path << ": " << e.what();
    }
}

std::optional<PrinterSettings> HydraSettings::get_for_preset(const std::string& preset_id) const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    auto                        it = m_printers.find(preset_id);
    if (it == m_printers.end())
        return std::nullopt;
    return it->second;
}

void HydraSettings::set_for_preset(const std::string& preset_id, const PrinterSettings& settings)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_printers[preset_id] = settings;
}

} // namespace Slic3r::Hydra

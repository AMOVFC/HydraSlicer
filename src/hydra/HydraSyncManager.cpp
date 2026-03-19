#include "HydraSyncManager.hpp"

#include "HydraPresetBundle.hpp"
#include "HydraSettings.hpp"
#include "HydraGitSync.hpp"

#include <boost/log/trivial.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <sstream>

namespace Slic3r::Hydra {

HydraSyncManager& HydraSyncManager::Instance()
{
    static HydraSyncManager instance;
    return instance;
}

void HydraSyncManager::configure_store_root(const std::string& root_path) { m_store_root = root_path; }

void HydraSyncManager::set_status_callback(std::function<void(SyncState, const std::string&)> cb)
{
    std::lock_guard<std::mutex> lk(m_callback_mutex);
    m_status_callback = std::move(cb);
}

void HydraSyncManager::set_state(SyncState state, const std::string& message)
{
    m_state = state;
    BOOST_LOG_TRIVIAL(info) << "Hydra sync state: " << message;
    std::lock_guard<std::mutex> lk(m_callback_mutex);
    if (m_status_callback)
        m_status_callback(state, message);
}

void HydraSyncManager::OnPrinterActivated(const std::string& printer_preset_id)
{
    std::thread([this, printer_preset_id]() {
        try {
            auto settings = HydraSettings::Instance().get_for_preset(printer_preset_id);
            if (!settings || !settings->auto_pull_on_activate || settings->hydra_printer_id.empty() || settings->moonraker_base_url.empty())
                return;

            HydraLocalStore      store(m_store_root);
            HydraMoonrakerClient client;
            const std::string    remote_root = "config/slicer/" + settings->hydra_printer_id;
            auto                 res         = client.download(settings->moonraker_base_url, remote_root + "/profile.orca_printer");
            if (!res.ok) {
                set_state(SyncState::SyncFailed, "Hydra pull failed: " + res.error);
                return;
            }

            const std::string hash = store.sha256_text(res.payload);
            const std::string dir  = store.cache_path_for_printer(settings->hydra_printer_id);
            const std::string path = dir + "/baseline.orca_printer";
            if (store.sha256_file(path) != hash) {
                store.atomic_write(path, res.payload);
                HydraPresetBundle::Instance().import_bundle(res.payload, printer_preset_id);
            }
            store.atomic_write(dir + "/baseline.sha256", hash);
            set_state(SyncState::InSync, "Hydra baseline pulled");
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Hydra OnPrinterActivated exception: " << e.what();
            set_state(SyncState::SyncFailed, std::string("Hydra pull exception: ") + e.what());
        }
    }).detach();
}

void HydraSyncManager::OnPresetsPossiblyChanged(const std::string& printer_preset_id)
{
    std::thread([this, printer_preset_id]() {
        try {
            auto settings = HydraSettings::Instance().get_for_preset(printer_preset_id);
            if (!settings || !settings->auto_push_on_export || settings->hydra_printer_id.empty())
                return;

            auto payload = HydraPresetBundle::Instance().export_bundle(printer_preset_id);
            if (!payload) {
                set_state(SyncState::SyncFailed, "Hydra export callback is not configured");
                return;
            }

            HydraLocalStore   store(m_store_root);
            const std::string hash = store.sha256_text(*payload);
            const std::string dir  = store.cache_path_for_printer(settings->hydra_printer_id);

            std::ifstream baseline_in(dir + "/baseline.sha256");
            std::string   baseline_hash;
            baseline_in >> baseline_hash;
            if (baseline_hash == hash) {
                set_state(SyncState::InSync, "Hydra already in sync");
                return;
            }

            const auto        now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            const std::string ts  = std::to_string(static_cast<long long>(now));

            if (!settings->moonraker_base_url.empty()) {
                HydraMoonrakerClient client;
                const std::string    root = "config/slicer/" + settings->hydra_printer_id;
                auto                 up   = client.upload(settings->moonraker_base_url, root + "/versions/" + ts + ".orca_printer", *payload);
                if (!up.ok) {
                    set_state(SyncState::SyncFailed, "Hydra push failed: " + up.error);
                    return;
                }

                boost::property_tree::ptree meta;
                meta.put("latest_hash", hash);
                meta.put("latest_timestamp", ts);
                std::ostringstream ss;
                boost::property_tree::write_json(ss, meta);
                client.upload(settings->moonraker_base_url, root + "/meta.json", ss.str());
            }

            if (settings->cloud_sync_mode == PrinterSettings::CloudSyncMode::GitRepository) {
                HydraGitSync git_sync(std::filesystem::path(m_store_root).append("git_sync").append(settings->hydra_printer_id).string(),
                                      settings->git_remote_url, settings->git_branch.empty() ? std::string("main") : settings->git_branch,
                                      settings->git_access_token);
                auto         git_result = git_sync.push_bundle(settings->hydra_printer_id, ts, *payload, hash);
                if (!git_result.ok) {
                    set_state(SyncState::SyncFailed, "Hydra git push failed: " + git_result.error);
                    return;
                }
            }

            if (settings->cloud_sync_mode == PrinterSettings::CloudSyncMode::SupabaseGithub) {
                if (settings->supabase_url.empty() || settings->supabase_anon_key.empty()) {
                    set_state(SyncState::SyncFailed, "Hydra Supabase sync is enabled but configuration is incomplete");
                    return;
                }
            }

            set_state(SyncState::PendingLocalChanges, "Hydra pushed version " + ts);
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Hydra OnPresetsPossiblyChanged exception: " << e.what();
            set_state(SyncState::SyncFailed, std::string("Hydra push exception: ") + e.what());
        }
    }).detach();
}

} // namespace Slic3r::Hydra

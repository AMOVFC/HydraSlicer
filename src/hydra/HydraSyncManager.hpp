#pragma once

#include "HydraLocalStore.hpp"
#include "HydraMoonrakerClient.hpp"

#include <atomic>
#include <functional>
#include <string>

namespace Slic3r::Hydra {

enum class SyncState {
    InSync,
    PendingLocalChanges,
    SyncFailed
};

class HydraSyncManager {
public:
    static HydraSyncManager &Instance();

    void configure_store_root(const std::string &root_path);
    void set_status_callback(std::function<void(SyncState, const std::string &)> cb);

    void OnPrinterActivated(const std::string &printer_preset_id);
    void OnPresetsPossiblyChanged(const std::string &printer_preset_id);

    SyncState state() const { return m_state.load(); }

private:
    void set_state(SyncState state, const std::string &message);

    std::atomic<SyncState>                         m_state {SyncState::InSync};
    std::function<void(SyncState, const std::string &)> m_status_callback;
    std::string                                    m_store_root;
};

} // namespace Slic3r::Hydra

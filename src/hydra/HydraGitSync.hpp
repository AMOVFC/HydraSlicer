#pragma once

#include <string>

namespace Slic3r::Hydra {

struct HydraGitSyncResult
{
    bool        ok = false;
    std::string error;
};

class HydraGitSync
{
public:
    HydraGitSync(std::string work_root, std::string remote_url, std::string branch, std::string access_token);

    HydraGitSyncResult test_connection() const;
    HydraGitSyncResult push_bundle(const std::string& printer_id,
                                   const std::string& timestamp,
                                   const std::string& bundle_payload,
                                   const std::string& bundle_hash) const;

private:
    HydraGitSyncResult ensure_repository() const;
    HydraGitSyncResult run_command(const std::string& command) const;
    std::string        tokenized_remote_url() const;

    std::string m_work_root;
    std::string m_remote_url;
    std::string m_branch;
    std::string m_access_token;
};

} // namespace Slic3r::Hydra

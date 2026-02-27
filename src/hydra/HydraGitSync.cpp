#include "HydraGitSync.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace Slic3r::Hydra {
namespace {

std::string shell_quote(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size() + 2);
    escaped += "'";
    for (const char c : text) {
        if (c == '\'')
            escaped += "'\\''";
        else
            escaped += c;
    }
    escaped += "'";
    return escaped;
}

} // namespace

HydraGitSync::HydraGitSync(std::string work_root, std::string remote_url, std::string branch, std::string access_token)
    : m_work_root(std::move(work_root))
    , m_remote_url(std::move(remote_url))
    , m_branch(std::move(branch))
    , m_access_token(std::move(access_token))
{}

HydraGitSyncResult HydraGitSync::run_command(const std::string& command) const
{
    const int rc = std::system(command.c_str());
    if (rc == 0)
        return {true, ""};

    std::ostringstream oss;
    oss << "command failed with exit code " << rc;
    return {false, oss.str()};
}

std::string HydraGitSync::tokenized_remote_url() const
{
    if (m_access_token.empty())
        return m_remote_url;
    static const std::string github_prefix = "https://github.com/";
    if (m_remote_url.rfind(github_prefix, 0) != 0)
        return m_remote_url;
    return "https://x-access-token:" + m_access_token + "@" + m_remote_url.substr(std::string("https://").size());
}

HydraGitSyncResult HydraGitSync::ensure_repository() const
{
    namespace fs = std::filesystem;
    fs::create_directories(m_work_root);

    const fs::path git_dir = fs::path(m_work_root) / ".git";
    if (!fs::exists(git_dir)) {
        auto init = run_command("git -C " + shell_quote(m_work_root) + " init");
        if (!init.ok)
            return init;
        auto checkout = run_command("git -C " + shell_quote(m_work_root) + " checkout -B " + shell_quote(m_branch));
        if (!checkout.ok)
            return checkout;
    }

    const std::string remote = tokenized_remote_url();
    if (!remote.empty()) {
        run_command("git -C " + shell_quote(m_work_root) + " remote remove origin >/dev/null 2>&1");
        auto add = run_command("git -C " + shell_quote(m_work_root) + " remote add origin " + shell_quote(remote));
        if (!add.ok)
            return add;
        run_command("git -C " + shell_quote(m_work_root) + " fetch origin " + shell_quote(m_branch) + " >/dev/null 2>&1");
    }

    run_command("git -C " + shell_quote(m_work_root) + " config user.email hydra@local.invalid");
    run_command("git -C " + shell_quote(m_work_root) + " config user.name HydraSync");
    return {true, ""};
}

HydraGitSyncResult HydraGitSync::test_connection() const
{
    if (m_remote_url.empty())
        return {false, "git remote URL is empty"};
    const std::string remote = tokenized_remote_url();
    return run_command("git ls-remote --heads " + shell_quote(remote) + " " + shell_quote(m_branch) + " >/dev/null 2>&1");
}

HydraGitSyncResult HydraGitSync::push_bundle(const std::string& printer_id,
                                             const std::string& timestamp,
                                             const std::string& bundle_payload,
                                             const std::string& bundle_hash) const
{
    auto repo = ensure_repository();
    if (!repo.ok)
        return repo;

    namespace fs               = std::filesystem;
    const fs::path printer_dir = fs::path(m_work_root) / "printers" / printer_id / "versions";
    fs::create_directories(printer_dir);

    const fs::path version_file = printer_dir / (timestamp + ".orca_printer");
    {
        std::ofstream out(version_file, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
            return {false, "failed to write bundle into local git repository"};
        out << bundle_payload;
    }

    boost::property_tree::ptree meta;
    meta.put("latest_hash", bundle_hash);
    meta.put("latest_timestamp", timestamp);

    const fs::path meta_file = fs::path(m_work_root) / "printers" / printer_id / "meta.json";
    fs::create_directories(meta_file.parent_path());
    boost::property_tree::write_json(meta_file.string(), meta);

    auto add = run_command("git -C " + shell_quote(m_work_root) + " add " + shell_quote(version_file.string()) + " " +
                           shell_quote(meta_file.string()));
    if (!add.ok)
        return add;

    auto commit = run_command("git -C " + shell_quote(m_work_root) + " commit -m " +
                              shell_quote("Hydra preset update " + printer_id + " @ " + timestamp) + " >/dev/null 2>&1");
    if (!commit.ok) {
        auto no_changes = run_command("git -C " + shell_quote(m_work_root) + " diff --cached --quiet");
        if (no_changes.ok)
            return {true, ""};
        return commit;
    }

    if (!m_remote_url.empty()) {
        auto push = run_command("git -C " + shell_quote(m_work_root) + " push -u origin " + shell_quote(m_branch) + " >/dev/null 2>&1");
        if (!push.ok)
            return push;
    }

    return {true, ""};
}

} // namespace Slic3r::Hydra

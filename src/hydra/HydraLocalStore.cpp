#include "HydraLocalStore.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>

namespace Slic3r::Hydra {

HydraLocalStore::HydraLocalStore(std::string root_path) : m_root_path(std::move(root_path))
{
}

std::string HydraLocalStore::cache_path_for_printer(const std::string &printer_id) const
{
    std::filesystem::path path = std::filesystem::path(m_root_path) / printer_id;
    std::filesystem::create_directories(path);
    return path.string();
}

bool HydraLocalStore::atomic_write(const std::string &path, const std::string &content) const
{
    const std::string tmp = path + ".tmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out << content;
    out.close();
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

std::string HydraLocalStore::sha256_text(const std::string &content) const
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(content.data()), content.size(), digest);
    std::ostringstream out;
    for (unsigned char i : digest) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(i);
    return out.str();
}

std::string HydraLocalStore::sha256_file(const std::string &path) const
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return sha256_text(ss.str());
}

} // namespace Slic3r::Hydra

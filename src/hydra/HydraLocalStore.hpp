#pragma once

#include <string>

namespace Slic3r::Hydra {

class HydraLocalStore {
public:
    explicit HydraLocalStore(std::string root_path);

    std::string cache_path_for_printer(const std::string &printer_id) const;
    bool        atomic_write(const std::string &path, const std::string &content) const;
    std::string sha256_text(const std::string &content) const;
    std::string sha256_file(const std::string &path) const;

private:
    std::string m_root_path;
};

} // namespace Slic3r::Hydra

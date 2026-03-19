#pragma once

#include <string>

namespace Slic3r::Hydra {

struct MoonrakerResult {
    bool        ok = false;
    int         http_status = 0;
    std::string payload;
    std::string error;
};

class HydraMoonrakerClient {
public:
    MoonrakerResult list(const std::string &base_url, const std::string &remote_path) const;
    MoonrakerResult download(const std::string &base_url, const std::string &remote_path) const;
    MoonrakerResult upload(const std::string &base_url, const std::string &remote_path, const std::string &content) const;
};

} // namespace Slic3r::Hydra

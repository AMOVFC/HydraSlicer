#include "HydraMoonrakerClient.hpp"

#include <curl/curl.h>

namespace Slic3r::Hydra {

static size_t append_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    const size_t total = size * nmemb;
    static_cast<std::string *>(userp)->append(static_cast<const char *>(contents), total);
    return total;
}

static MoonrakerResult request(const std::string &url, const char *method, const std::string *body = nullptr)
{
    MoonrakerResult out;
    CURL          *curl = curl_easy_init();
    if (!curl) {
        out.error = "curl init failed";
        return out;
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    struct curl_slist *headers = nullptr;
    if (body != nullptr) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
        headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    const CURLcode code = curl_easy_perform(curl);
    if (headers) curl_slist_free_all(headers);
    if (code != CURLE_OK) {
        out.error = curl_easy_strerror(code);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        out.http_status = static_cast<int>(http_code);
        if (http_code >= 200 && http_code < 300) {
            out.ok = true;
            out.payload = std::move(response);
        } else {
            out.error = "HTTP " + std::to_string(http_code);
            out.payload = std::move(response);
        }
    }
    curl_easy_cleanup(curl);
    return out;
}

MoonrakerResult HydraMoonrakerClient::list(const std::string &base_url, const std::string &remote_path) const
{
    return request(base_url + "/server/files/list?root=" + remote_path, "GET");
}

MoonrakerResult HydraMoonrakerClient::download(const std::string &base_url, const std::string &remote_path) const
{
    return request(base_url + "/server/files/" + remote_path, "GET");
}

MoonrakerResult HydraMoonrakerClient::upload(const std::string &base_url, const std::string &remote_path, const std::string &content) const
{
    return request(base_url + "/server/files/upload?path=" + remote_path, "POST", &content);
}

} // namespace Slic3r::Hydra

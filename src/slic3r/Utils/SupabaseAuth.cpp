#include "SupabaseAuth.hpp"
#include "Http.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"

#include <random>
#include <sstream>
#include <iomanip>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace Slic3r {

// Base64url encoding (no padding, URL-safe)
static std::string base64url_encode(const unsigned char* data, size_t len)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string result;
    result.reserve(4 * ((len + 2) / 3));

    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = (unsigned int)data[i] << 16;
        if (i + 1 < len) n |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < len) n |= (unsigned int)data[i + 2];

        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        if (i + 1 < len) result += table[(n >> 6) & 0x3F];
        if (i + 2 < len) result += table[n & 0x3F];
    }
    return result;
}

static std::string generate_random_string(size_t length)
{
    std::vector<unsigned char> buf(length);
    RAND_bytes(buf.data(), static_cast<int>(length));
    return base64url_encode(buf.data(), buf.size());
}

// Decode base64 (standard, not URL-safe, for JWT parsing)
static std::string base64_decode(const std::string& input)
{
    // Convert base64url to standard base64
    std::string b64 = input;
    for (auto& c : b64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    // Add padding
    while (b64.size() % 4 != 0) b64 += '=';

    BIO* bio = BIO_new_mem_buf(b64.data(), static_cast<int>(b64.size()));
    BIO* b64bio = BIO_new(BIO_f_base64());
    bio = BIO_push(b64bio, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::vector<char> out(b64.size());
    int decoded_len = BIO_read(bio, out.data(), static_cast<int>(out.size()));
    BIO_free_all(bio);

    if (decoded_len <= 0) return "";
    return std::string(out.data(), decoded_len);
}

SupabaseAuth::SupabaseAuth() = default;
SupabaseAuth::~SupabaseAuth() = default;

void SupabaseAuth::configure(const SupabaseConfig& config)
{
    m_config = config;
    BOOST_LOG_TRIVIAL(info) << "SupabaseAuth configured: url=" << config.project_url
                            << " port=" << config.callback_port;
}

void SupabaseAuth::load_config_from_app(AppConfig* app_config)
{
    if (!app_config) return;

    SupabaseConfig config;
    config.project_url = app_config->get(SETTING_SUPABASE_URL);
    config.anon_key = app_config->get(SETTING_SUPABASE_ANON_KEY);

    std::string port_str = app_config->get(SETTING_SUPABASE_CALLBACK_PORT);
    if (!port_str.empty()) {
        try { config.callback_port = std::stoi(port_str); } catch (...) {}
    }

    if (!config.project_url.empty() && !config.anon_key.empty()) {
        configure(config);
    }

    m_config_dir = Slic3r::data_dir();
}

std::string SupabaseAuth::provider_to_string(AuthProvider provider)
{
    switch (provider) {
        case AuthProvider::GitHub: return "github";
        case AuthProvider::Google: return "google";
    }
    return "github";
}

std::string SupabaseAuth::provider_display_name(AuthProvider provider)
{
    switch (provider) {
        case AuthProvider::GitHub: return "GitHub";
        case AuthProvider::Google: return "Google";
    }
    return "GitHub";
}

SupabaseAuth::PkceChallenge SupabaseAuth::generate_pkce()
{
    PkceChallenge pkce;

    // Generate a 32-byte random verifier, base64url-encoded
    pkce.verifier = generate_random_string(32);

    // SHA-256 hash of the verifier for the challenge
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(pkce.verifier.data()),
           pkce.verifier.size(), hash);
    pkce.challenge = base64url_encode(hash, SHA256_DIGEST_LENGTH);

    // Random state for CSRF protection
    pkce.state = generate_random_string(16);

    return pkce;
}

std::string SupabaseAuth::get_oauth_url(AuthProvider provider)
{
    m_current_pkce = generate_pkce();

    std::string url = m_config.auth_url() + "/authorize";
    url += "?provider=" + provider_to_string(provider);
    url += "&redirect_to=" + m_config.callback_url();
    url += "&code_challenge=" + m_current_pkce.challenge;
    url += "&code_challenge_method=S256";
    url += "&state=" + m_current_pkce.state;

    BOOST_LOG_TRIVIAL(info) << "SupabaseAuth OAuth URL for " << provider_display_name(provider)
                            << ": " << url;
    return url;
}

void SupabaseAuth::start_login(AuthProvider provider, OnLoginComplete on_complete)
{
    // This generates the URL and stores the PKCE challenge.
    // The actual browser opening and callback handling is done by the GUI layer
    // (HydraLoginDialog) using OAuthJob.
    std::string url = get_oauth_url(provider);

    BOOST_LOG_TRIVIAL(info) << "SupabaseAuth: login flow started for " << provider_display_name(provider);

    // The on_complete callback will be called by the GUI layer after
    // the OAuth flow completes and exchange_code_for_session succeeds.
    (void)on_complete;
}

bool SupabaseAuth::exchange_code_for_session(const std::string& code)
{
    // Exchange authorization code for session tokens using Supabase token endpoint
    std::string token_url = m_config.auth_url() + "/token?grant_type=pkce";

    nlohmann::json body;
    body["auth_code"] = code;
    body["code_verifier"] = m_current_pkce.verifier;

    std::string response_body;
    bool success = false;

    auto http = Http::post(token_url);
    http.timeout_connect(10)
        .timeout_max(30)
        .header("apikey", m_config.anon_key)
        .header("Content-Type", "application/json")
        .set_post_body(body.dump())
        .on_complete([&](std::string body, unsigned status) {
            SupabaseSession new_session;
            if (parse_auth_response(body, new_session)) {
                std::lock_guard<std::mutex> lock(m_session_mutex);
                m_session = new_session;
                m_session.logged_in = true;
                success = true;
                BOOST_LOG_TRIVIAL(info) << "SupabaseAuth: login successful, user_id=" << m_session.user_id;
            } else {
                BOOST_LOG_TRIVIAL(error) << "SupabaseAuth: failed to parse token response";
            }
        })
        .on_error([&](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << "SupabaseAuth: token exchange failed: " << error
                                     << " status=" << status << " body=" << body;
        })
        .perform_sync();

    if (success) {
        persist_session();
        if (m_on_session_change) {
            m_on_session_change(m_session);
        }
    }

    return success;
}

bool SupabaseAuth::refresh_access_token()
{
    std::string refresh_token;
    {
        std::lock_guard<std::mutex> lock(m_session_mutex);
        if (m_session.refresh_token.empty()) return false;
        refresh_token = m_session.refresh_token;
    }

    std::string token_url = m_config.auth_url() + "/token?grant_type=refresh_token";

    nlohmann::json body;
    body["refresh_token"] = refresh_token;

    std::string response_body;
    bool success = false;

    auto http = Http::post(token_url);
    http.timeout_connect(10)
        .timeout_max(30)
        .header("apikey", m_config.anon_key)
        .header("Content-Type", "application/json")
        .set_post_body(body.dump())
        .on_complete([&](std::string body, unsigned status) {
            SupabaseSession new_session;
            if (parse_auth_response(body, new_session)) {
                std::lock_guard<std::mutex> lock(m_session_mutex);
                // Preserve fields not in token response
                new_session.provider = m_session.provider;
                m_session = new_session;
                m_session.logged_in = true;
                success = true;
                BOOST_LOG_TRIVIAL(info) << "SupabaseAuth: token refreshed successfully";
            }
        })
        .on_error([&](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << "SupabaseAuth: token refresh failed: " << error;
            if (status == 401 || status == 403) {
                // Refresh token expired, need re-login
                std::lock_guard<std::mutex> lock(m_session_mutex);
                m_session = SupabaseSession{};
            }
        })
        .perform_sync();

    if (success) {
        persist_session();
    }

    return success;
}

bool SupabaseAuth::is_logged_in() const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_session.logged_in;
}

bool SupabaseAuth::is_token_expired() const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    if (!m_session.logged_in) return true;
    auto now = std::chrono::system_clock::now();
    // Consider expired if less than 60 seconds remaining
    return (m_session.expires_at - now) < std::chrono::seconds(60);
}

const SupabaseSession& SupabaseAuth::get_session() const
{
    return m_session;
}

std::string SupabaseAuth::get_access_token() const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_session.access_token;
}

std::string SupabaseAuth::get_user_id() const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_session.user_id;
}

std::string SupabaseAuth::get_display_name() const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_session.display_name;
}

std::string SupabaseAuth::get_email() const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_session.email;
}

std::string SupabaseAuth::get_avatar_url() const
{
    std::lock_guard<std::mutex> lock(m_session_mutex);
    return m_session.avatar_url;
}

void SupabaseAuth::logout()
{
    std::string access_token;
    {
        std::lock_guard<std::mutex> lock(m_session_mutex);
        access_token = m_session.access_token;
    }

    // Call Supabase logout endpoint
    if (!access_token.empty()) {
        std::string logout_url = m_config.auth_url() + "/logout";
        auto http = Http::post(logout_url);
        http.timeout_connect(5)
            .timeout_max(10)
            .header("apikey", m_config.anon_key)
            .header("Authorization", "Bearer " + access_token)
            .on_complete([](std::string, unsigned) {
                BOOST_LOG_TRIVIAL(info) << "SupabaseAuth: logout request sent";
            })
            .on_error([](std::string, std::string error, unsigned) {
                BOOST_LOG_TRIVIAL(warning) << "SupabaseAuth: logout request failed: " << error;
            })
            .perform_sync();
    }

    {
        std::lock_guard<std::mutex> lock(m_session_mutex);
        m_session = SupabaseSession{};
    }

    // Clear persisted session
    if (!m_config_dir.empty()) {
        boost::filesystem::path session_path = boost::filesystem::path(m_config_dir) / "hydra_session.json";
        boost::filesystem::remove(session_path);
    }

    if (m_on_session_change) {
        m_on_session_change(m_session);
    }

    BOOST_LOG_TRIVIAL(info) << "SupabaseAuth: logged out";
}

void SupabaseAuth::persist_session()
{
    if (m_config_dir.empty()) return;

    std::lock_guard<std::mutex> lock(m_session_mutex);
    if (!m_session.logged_in) return;

    nlohmann::json j;
    j["refresh_token"] = m_session.refresh_token;
    j["user_id"] = m_session.user_id;
    j["email"] = m_session.email;
    j["display_name"] = m_session.display_name;
    j["avatar_url"] = m_session.avatar_url;
    j["provider"] = m_session.provider;

    boost::filesystem::path session_path = boost::filesystem::path(m_config_dir) / "hydra_session.json";
    boost::nowide::ofstream ofs(session_path.string());
    if (ofs.is_open()) {
        ofs << j.dump(2);
        BOOST_LOG_TRIVIAL(info) << "SupabaseAuth: session persisted";
    }
}

bool SupabaseAuth::load_persisted_session()
{
    if (m_config_dir.empty()) return false;

    boost::filesystem::path session_path = boost::filesystem::path(m_config_dir) / "hydra_session.json";
    if (!boost::filesystem::exists(session_path)) return false;

    boost::nowide::ifstream ifs(session_path.string());
    if (!ifs.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    auto j = nlohmann::json::parse(content, nullptr, false);
    if (j.is_discarded()) return false;

    std::string refresh_token;
    if (j.contains("refresh_token")) j.at("refresh_token").get_to(refresh_token);
    if (refresh_token.empty()) return false;

    // Try to refresh the session using the stored refresh token
    {
        std::lock_guard<std::mutex> lock(m_session_mutex);
        m_session.refresh_token = refresh_token;
        if (j.contains("user_id")) j.at("user_id").get_to(m_session.user_id);
        if (j.contains("email")) j.at("email").get_to(m_session.email);
        if (j.contains("display_name")) j.at("display_name").get_to(m_session.display_name);
        if (j.contains("avatar_url")) j.at("avatar_url").get_to(m_session.avatar_url);
        if (j.contains("provider")) j.at("provider").get_to(m_session.provider);
    }

    // Attempt token refresh to get a fresh access token
    if (refresh_access_token()) {
        BOOST_LOG_TRIVIAL(info) << "SupabaseAuth: restored session from persisted state";
        return true;
    }

    BOOST_LOG_TRIVIAL(warning) << "SupabaseAuth: failed to restore session, refresh token may be expired";
    return false;
}

void SupabaseAuth::set_on_session_change(OnSessionChange callback)
{
    m_on_session_change = std::move(callback);
}

bool SupabaseAuth::decode_jwt_expiry(const std::string& token, std::chrono::system_clock::time_point& out)
{
    // JWT is three base64url-encoded parts separated by dots
    auto first_dot = token.find('.');
    if (first_dot == std::string::npos) return false;
    auto second_dot = token.find('.', first_dot + 1);
    if (second_dot == std::string::npos) return false;

    std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    std::string payload = base64_decode(payload_b64);

    auto j = nlohmann::json::parse(payload, nullptr, false);
    if (j.is_discarded() || !j.contains("exp")) return false;

    int64_t exp = j["exp"].get<int64_t>();
    out = std::chrono::system_clock::from_time_t(static_cast<time_t>(exp));
    return true;
}

bool SupabaseAuth::parse_auth_response(const std::string& json_body, SupabaseSession& session)
{
    auto j = nlohmann::json::parse(json_body, nullptr, false);
    if (j.is_discarded()) return false;

    if (j.contains("access_token")) j.at("access_token").get_to(session.access_token);
    if (j.contains("refresh_token")) j.at("refresh_token").get_to(session.refresh_token);

    if (session.access_token.empty()) return false;

    // Decode expiry from JWT
    decode_jwt_expiry(session.access_token, session.expires_at);

    // Extract user info from the response
    if (j.contains("user")) {
        auto& user = j["user"];
        if (user.contains("id")) user.at("id").get_to(session.user_id);
        if (user.contains("email")) user.at("email").get_to(session.email);

        // Extract display name and avatar from user_metadata
        if (user.contains("user_metadata")) {
            auto& meta = user["user_metadata"];
            if (meta.contains("full_name")) meta.at("full_name").get_to(session.display_name);
            else if (meta.contains("name")) meta.at("name").get_to(session.display_name);
            else if (meta.contains("user_name")) meta.at("user_name").get_to(session.display_name);

            if (meta.contains("avatar_url")) meta.at("avatar_url").get_to(session.avatar_url);
            if (meta.contains("picture")) meta.at("picture").get_to(session.avatar_url);
        }

        // Extract provider
        if (user.contains("app_metadata")) {
            auto& app_meta = user["app_metadata"];
            if (app_meta.contains("provider")) app_meta.at("provider").get_to(session.provider);
        }
    }

    return true;
}

} // namespace Slic3r

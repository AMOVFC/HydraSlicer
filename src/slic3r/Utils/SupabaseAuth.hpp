#ifndef __SUPABASE_AUTH_HPP__
#define __SUPABASE_AUTH_HPP__

#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace Slic3r {

class AppConfig;

// Supported OAuth providers for HydraSlicer
enum class AuthProvider {
    GitHub,
    Google
};

// User session info from Supabase
struct SupabaseSession {
    std::string access_token;
    std::string refresh_token;
    std::string user_id;
    std::string email;
    std::string display_name;
    std::string avatar_url;
    std::string provider;    // "github" or "google"
    std::chrono::system_clock::time_point expires_at{};
    bool logged_in = false;
};

// Configuration for the Supabase project
struct SupabaseConfig {
    std::string project_url;   // e.g. "https://xxxxx.supabase.co"
    std::string anon_key;      // Supabase anon/public key
    int callback_port = 41173; // Local OAuth callback port

    std::string auth_url() const { return project_url + "/auth/v1"; }
    std::string rest_url() const { return project_url + "/rest/v1"; }
    std::string callback_url() const {
        return "http://localhost:" + std::to_string(callback_port) + "/auth/callback";
    }
};

/**
 * SupabaseAuth - Authentication service for HydraSlicer using Supabase.
 *
 * Supports OAuth login via GitHub and Google through Supabase Auth.
 * Uses the PKCE flow with a local loopback server to receive the callback.
 *
 * Flow:
 *  1. User clicks "Sign in with GitHub/Google"
 *  2. Browser opens Supabase OAuth URL with PKCE challenge
 *  3. User authenticates with the provider
 *  4. Supabase redirects to local callback server
 *  5. We exchange the auth code for tokens
 *  6. Session is established with access + refresh tokens
 */
class SupabaseAuth {
public:
    using OnLoginComplete = std::function<void(bool success, const std::string& error)>;
    using OnSessionChange = std::function<void(const SupabaseSession& session)>;

    SupabaseAuth();
    ~SupabaseAuth();

    // Configuration
    void configure(const SupabaseConfig& config);
    void load_config_from_app(AppConfig* app_config);
    const SupabaseConfig& get_config() const { return m_config; }

    // OAuth login flow
    // Returns the URL to open in the browser
    std::string get_oauth_url(AuthProvider provider);

    // Start the OAuth flow: opens browser and starts local callback server
    void start_login(AuthProvider provider, OnLoginComplete on_complete);

    // Token management
    bool refresh_access_token();
    bool is_logged_in() const;
    bool is_token_expired() const;

    // Session access
    const SupabaseSession& get_session() const;
    std::string get_access_token() const;
    std::string get_user_id() const;
    std::string get_display_name() const;
    std::string get_email() const;
    std::string get_avatar_url() const;

    // Logout
    void logout();

    // Persistence
    void persist_session();
    bool load_persisted_session();

    // Callbacks
    void set_on_session_change(OnSessionChange callback);

    // Provider name helpers
    static std::string provider_to_string(AuthProvider provider);
    static std::string provider_display_name(AuthProvider provider);

private:
    // PKCE helpers
    struct PkceChallenge {
        std::string verifier;
        std::string challenge;
        std::string state;
    };

    PkceChallenge generate_pkce();

    // Token exchange
    bool exchange_code_for_session(const std::string& code);

    // JWT decoding
    bool decode_jwt_expiry(const std::string& token, std::chrono::system_clock::time_point& out);

    // Parse Supabase auth response
    bool parse_auth_response(const std::string& json_body, SupabaseSession& session);

    SupabaseConfig m_config;
    SupabaseSession m_session;
    PkceChallenge m_current_pkce;
    mutable std::mutex m_session_mutex;

    OnSessionChange m_on_session_change;
    std::string m_config_dir;
};

} // namespace Slic3r

#endif // __SUPABASE_AUTH_HPP__

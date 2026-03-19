#include "HydraLoginDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "Jobs/BoostThreadWorker.hpp"
#include "Jobs/PlaterWorker.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/DialogButtons.hpp"

#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_HYDRA_LOGIN_COMPLETE, wxCommandEvent);

#define BORDER_W FromDIP(10)

HydraLoginDialog::HydraLoginDialog(wxWindow* parent, SupabaseAuth& auth)
    : DPIDialog(parent, wxID_ANY, _L("Sign in to HydraSlicer"),
                wxDefaultPosition, wxSize(-1, -1),
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_auth(auth)
{
    SetFont(wxGetApp().normal_font());
    SetBackgroundColour(*wxWHITE);

    m_worker = std::make_unique<PlaterWorker<BoostThreadWorker>>(this, nullptr, "hydra_auth_worker");

    m_main_sizer = new wxBoxSizer(wxVERTICAL);

    // Title / Header
    auto* title = new wxStaticText(this, wxID_ANY, _L("Welcome to HydraSlicer"));
    title->SetFont(wxGetApp().bold_font().Scaled(1.4f));
    title->SetForegroundColour(*wxBLACK);
    m_main_sizer->Add(title, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxLEFT | wxRIGHT, FromDIP(30));

    auto* subtitle = new wxStaticText(this, wxID_ANY, _L("Sign in to sync settings and connect to your printers"));
    subtitle->SetForegroundColour(wxColour(100, 100, 100));
    m_main_sizer->Add(subtitle, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxLEFT | wxRIGHT, FromDIP(10));

    m_main_sizer->AddSpacer(FromDIP(25));

    // Provider buttons panel
    m_button_panel = new wxPanel(this);
    auto* btn_sizer = new wxBoxSizer(wxVERTICAL);

    // GitHub button
    auto* github_btn = new Button(m_button_panel, _L("Sign in with GitHub"));
    github_btn->SetMinSize(wxSize(FromDIP(300), FromDIP(40)));
    github_btn->SetBackgroundColour(wxColour(36, 41, 47));    // GitHub dark
    github_btn->SetTextColor(wxColour(255, 255, 255));
    github_btn->SetCornerRadius(FromDIP(6));
    github_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        start_oauth(AuthProvider::GitHub);
    });
    btn_sizer->Add(github_btn, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));

    btn_sizer->AddSpacer(FromDIP(12));

    // Google button
    auto* google_btn = new Button(m_button_panel, _L("Sign in with Google"));
    google_btn->SetMinSize(wxSize(FromDIP(300), FromDIP(40)));
    google_btn->SetBackgroundColour(wxColour(255, 255, 255));
    google_btn->SetTextColor(wxColour(60, 60, 60));
    google_btn->SetBorderColor(wxColour(200, 200, 200));
    google_btn->SetCornerRadius(FromDIP(6));
    google_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        start_oauth(AuthProvider::Google);
    });
    btn_sizer->Add(google_btn, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));

    m_button_panel->SetSizer(btn_sizer);
    m_main_sizer->Add(m_button_panel, 0, wxEXPAND);

    // Authorizing panel (hidden initially)
    m_auth_panel = new wxPanel(this);
    auto* auth_sizer = new wxBoxSizer(wxVERTICAL);

    auto* auth_msg = new wxStaticText(m_auth_panel, wxID_ANY, _L("Authorizing in your browser..."));
    auth_msg->SetForegroundColour(*wxBLACK);
    auth_sizer->Add(auth_msg, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, FromDIP(20));

    auto* cancel_btn = new Button(m_auth_panel, _L("Cancel"));
    cancel_btn->SetMinSize(wxSize(FromDIP(120), FromDIP(36)));
    cancel_btn->Bind(wxEVT_BUTTON, &HydraLoginDialog::on_cancel, this);
    auth_sizer->Add(cancel_btn, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, FromDIP(10));

    m_auth_panel->SetSizer(auth_sizer);
    m_auth_panel->Hide();
    m_main_sizer->Add(m_auth_panel, 0, wxEXPAND);

    m_main_sizer->AddSpacer(FromDIP(20));

    // Skip / close link
    auto* skip_text = new wxStaticText(this, wxID_ANY, _L("Skip for now"));
    skip_text->SetForegroundColour(wxColour(80, 80, 200));
    skip_text->SetCursor(wxCursor(wxCURSOR_HAND));
    skip_text->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
        if (IsModal()) EndModal(wxID_CANCEL);
        else Close();
    });
    m_main_sizer->Add(skip_text, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, FromDIP(20));

    SetSizer(m_main_sizer);
    m_main_sizer->Fit(this);

    Bind(wxEVT_CLOSE_WINDOW, &HydraLoginDialog::on_cancel, this);
    Bind(EVT_OAUTH_COMPLETE_MESSAGE, &HydraLoginDialog::on_oauth_complete, this);

    CenterOnParent();
    wxGetApp().UpdateDlgDarkUI(this);
}

HydraLoginDialog::~HydraLoginDialog() = default;

void HydraLoginDialog::start_oauth(AuthProvider provider)
{
    m_selected_provider = provider;

    // Get the OAuth URL from SupabaseAuth (generates PKCE)
    std::string oauth_url = m_auth.get_oauth_url(provider);
    const auto& config = m_auth.get_config();

    // Set up OAuthParams for the OAuthJob
    OAuthParams params;
    params.login_url = oauth_url;
    params.client_id = "";  // Supabase handles this server-side
    params.callback_port = static_cast<boost::asio::ip::port_type>(config.callback_port);
    params.callback_url = config.callback_url();
    params.scope = "";
    params.response_type = "code";
    params.auth_success_redirect_url = config.project_url + "/auth/v1/authorized";
    params.auth_fail_redirect_url = config.project_url + "/auth/v1/unauthorized";
    params.token_url = config.auth_url() + "/token?grant_type=pkce";
    params.verification_code = ""; // PKCE handled by SupabaseAuth
    params.state = ""; // State handled by SupabaseAuth

    // Build the OAuthJob
    m_result_ptr = std::make_shared<OAuthResult>();
    auto job = std::make_unique<OAuthJob>(OAuthData{params, m_result_ptr});
    job->set_event_handle(this);

    // Switch UI to authorizing state
    show_authorizing_state();

    // Start the job
    replace_job(*m_worker, std::move(job));

    // Open browser
    wxLaunchDefaultBrowser(oauth_url);

    BOOST_LOG_TRIVIAL(info) << "HydraLoginDialog: started OAuth for "
                            << SupabaseAuth::provider_display_name(provider);
}

void HydraLoginDialog::on_oauth_complete(wxCommandEvent& evt)
{
    if (m_result_ptr) {
        m_result = *m_result_ptr;
    }

    if (m_result.success) {
        BOOST_LOG_TRIVIAL(info) << "HydraLoginDialog: OAuth completed successfully";

        // Exchange the tokens with SupabaseAuth for session management
        // The OAuthJob already exchanged the code for tokens via the token_url.
        // We need to tell SupabaseAuth about the new session.
        // For now, the access/refresh tokens from OAuthResult are the Supabase tokens.
        // TODO: Parse the full Supabase session response for user info.

        wxCommandEvent login_evt(EVT_HYDRA_LOGIN_COMPLETE);
        login_evt.SetInt(1); // success
        wxPostEvent(GetParent(), login_evt);

        if (IsModal()) EndModal(wxID_OK);
        else Close();
    } else {
        // Show error and return to button state
        BOOST_LOG_TRIVIAL(warning) << "HydraLoginDialog: OAuth failed: " << m_result.error_message;
        show_provider_buttons();

        if (!m_result.error_message.empty()) {
            wxMessageBox(wxString::FromUTF8(m_result.error_message),
                         _L("Login Failed"), wxOK | wxICON_ERROR, this);
        }
    }
}

void HydraLoginDialog::on_cancel(wxEvent& evt)
{
    if (m_worker) {
        m_worker->cancel_all();
        m_worker->wait_for_idle();
    }

    if (IsModal()) EndModal(wxID_CANCEL);
    else Close();
}

void HydraLoginDialog::show_authorizing_state()
{
    m_button_panel->Hide();
    m_auth_panel->Show();
    m_main_sizer->Layout();
    Fit();
}

void HydraLoginDialog::show_provider_buttons()
{
    m_auth_panel->Hide();
    m_button_panel->Show();
    m_main_sizer->Layout();
    Fit();
}

void HydraLoginDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    Fit();
    Refresh();
}

}} // namespace Slic3r::GUI

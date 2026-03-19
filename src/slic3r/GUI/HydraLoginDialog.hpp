#ifndef slic3r_HydraLoginDialog_hpp_
#define slic3r_HydraLoginDialog_hpp_

#include "GUI_Utils.hpp"
#include "Jobs/OAuthJob.hpp"
#include "Jobs/Worker.hpp"
#include "slic3r/Utils/SupabaseAuth.hpp"

namespace Slic3r { namespace GUI {

wxDECLARE_EVENT(EVT_HYDRA_LOGIN_COMPLETE, wxCommandEvent);

/**
 * HydraLoginDialog - Login dialog for HydraSlicer with GitHub and Google OAuth.
 *
 * Displays provider buttons. When clicked, opens the browser for OAuth flow
 * and shows "Authorizing..." while waiting for the callback.
 */
class HydraLoginDialog : public DPIDialog
{
public:
    HydraLoginDialog(wxWindow* parent, SupabaseAuth& auth);
    ~HydraLoginDialog();

    OAuthResult get_result() const { return m_result; }
    AuthProvider get_selected_provider() const { return m_selected_provider; }

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    void start_oauth(AuthProvider provider);
    void on_oauth_complete(wxCommandEvent& evt);
    void on_cancel(wxEvent& evt);
    void show_authorizing_state();
    void show_provider_buttons();

    SupabaseAuth& m_auth;
    AuthProvider m_selected_provider{AuthProvider::GitHub};
    OAuthResult m_result;
    std::shared_ptr<OAuthResult> m_result_ptr;
    std::unique_ptr<Worker> m_worker;

    // UI elements
    wxBoxSizer* m_main_sizer{nullptr};
    wxPanel* m_button_panel{nullptr};
    wxPanel* m_auth_panel{nullptr};
};

}} // namespace Slic3r::GUI

#endif // slic3r_HydraLoginDialog_hpp_

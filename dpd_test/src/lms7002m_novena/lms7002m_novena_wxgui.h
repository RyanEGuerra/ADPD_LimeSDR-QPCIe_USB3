/**
@file	lms7002m_novena_wxgui.h
@author Lime Microsystems (www.limemicro.com)
@brief	User interface for controls on Novena board
*/

#ifndef LMS7002M_NOVENA_WXGUI_H
#define LMS7002M_NOVENA_WXGUI_H

#include <wx/wx.h>
namespace lime{
class IConnection;
}

class LMS7002M_Novena_wxgui : public wxFrame
{
public:
    LMS7002M_Novena_wxgui(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString &title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long styles = 0);
    void Initialize(lime::IConnection* serPort, const size_t devIndex = 0);
    virtual ~LMS7002M_Novena_wxgui();
    virtual void UpdatePanel();

    wxCheckBox *lms_reset, *lms_rxen, *lms_txen, *lms_gpio2, *lms_gpio1, *lms_gpio0;
    wxButton* btnReadAll;
protected:
    void ParameterChangeHandler(wxCommandEvent& event);
    void OnReadAll(wxCommandEvent& event);
    lime::IConnection* mSerPort;
    int m_rficSpiAddr;

};

#endif //LMS7002M_NOVENA_H

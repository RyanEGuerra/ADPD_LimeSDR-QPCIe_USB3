#include "lms7002_pnlRBB_view.h"
#include "LMS7002M.h"
#include "ErrorReporting.h"
#include <map>
#include <wx/msgdlg.h>
#include "numericSlider.h"
#include "lms7002_gui_utilities.h"
#include "lms7suiteEvents.h"
using namespace lime;

lms7002_pnlRBB_view::lms7002_pnlRBB_view( wxWindow* parent )
:
pnlRBB_view( parent )
{

}

lms7002_pnlRBB_view::lms7002_pnlRBB_view( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style )
    : pnlRBB_view(parent, id, pos, size, style), lmsControl(nullptr)
{
    wndId2Enum[cmbC_CTL_LPFH_RBB] = C_CTL_LPFH_RBB;
    wndId2Enum[cmbC_CTL_LPFL_RBB] = C_CTL_LPFL_RBB;
    wndId2Enum[cmbC_CTL_PGA_RBB] = C_CTL_PGA_RBB;
    wndId2Enum[chkEN_G_RBB] = EN_G_RBB;
    wndId2Enum[cmbG_PGA_RBB] = G_PGA_RBB;
    wndId2Enum[cmbICT_LPF_IN_RBB] = ICT_LPF_IN_RBB;
    wndId2Enum[cmbICT_LPF_OUT_RBB] = ICT_LPF_OUT_RBB;
    wndId2Enum[cmbICT_PGA_IN_RBB] = ICT_PGA_IN_RBB;
    wndId2Enum[cmbICT_PGA_OUT_RBB] = ICT_PGA_OUT_RBB;
    wndId2Enum[cmbINPUT_CTL_PGA_RBB] = INPUT_CTL_PGA_RBB;
    wndId2Enum[rgrOSW_PGA_RBB] = OSW_PGA_RBB;
    wndId2Enum[chkPD_LPFH_RBB] = PD_LPFH_RBB;
    wndId2Enum[chkPD_LPFL_RBB] = PD_LPFL_RBB;
    wndId2Enum[chkPD_PGA_RBB] = PD_PGA_RBB;
    wndId2Enum[cmbRCC_CTL_LPFH_RBB] = RCC_CTL_LPFH_RBB;
    wndId2Enum[cmbRCC_CTL_LPFL_RBB] = RCC_CTL_LPFL_RBB;
    wndId2Enum[cmbRCC_CTL_PGA_RBB] = RCC_CTL_PGA_RBB;
    wndId2Enum[cmbR_CTL_LPF_RBB] = R_CTL_LPF_RBB;
    wndId2Enum[chkEN_DIR_RBB] = EN_DIR_RBB;


    wxArrayString temp;
    temp.clear();
    for (int i = 0; i<32; ++i)
    {
        temp.push_back(wxString::Format(_("%i dB"), -12 + i));
    }
    cmbG_PGA_RBB->Set(temp);

    temp.clear();
    for (int i = 0; i<32; ++i)
    {
        temp.push_back(wxString::Format(_("%i"), i));
    }
    cmbICT_LPF_IN_RBB->Set(temp);
    cmbICT_LPF_OUT_RBB->Set(temp);
    cmbICT_PGA_IN_RBB->Set(temp);
    cmbICT_PGA_OUT_RBB->Set(temp);
    cmbRCC_CTL_PGA_RBB->Append(temp);
    cmbR_CTL_LPF_RBB->Append(temp);

    temp.clear();
    temp.push_back("LPFL_RBB");
    temp.push_back("LPFH_RBB");
    temp.push_back("LPF bypass");
    temp.push_back("Loopback from TX");
    temp.push_back("Loopback path from peak detector");
    cmbINPUT_CTL_PGA_RBB->Set(temp);

    temp.clear();
    for (int i = 0; i<8; ++i)
    {
        temp.push_back(wxString::Format(_("%i"), i));
    }
    cmbRCC_CTL_LPFH_RBB->Set(temp);

    temp.clear();
    temp.push_back("when rxMode is 1.4MHz");
    temp.push_back("when 3MHz");
    temp.push_back("when 5MHz");
    temp.push_back("when 10MHz");
    temp.push_back("when 15MHz");
    temp.push_back("when 20MHz");
    cmbRCC_CTL_LPFL_RBB->Set(temp);

    LMS7002_WXGUI::UpdateTooltips(wndId2Enum, true);
}

void lms7002_pnlRBB_view::Initialize(LMS7002M* pControl)
{
    lmsControl = pControl;
    assert(lmsControl != nullptr);
}

void lms7002_pnlRBB_view::ParameterChangeHandler(wxSpinEvent& event)
{
    wxCommandEvent evt;
    evt.SetInt(event.GetInt());
    evt.SetId(event.GetId());
    evt.SetEventObject(event.GetEventObject());
    ParameterChangeHandler(evt);
}

void lms7002_pnlRBB_view::ParameterChangeHandler(wxCommandEvent& event)
{
    assert(lmsControl != nullptr);
    LMS7Parameter parameter;
    try
    {
        parameter = wndId2Enum.at(reinterpret_cast<wxWindow*>(event.GetEventObject()));
    }
    catch (std::exception & e)
    {
        std::cout << "Control element(ID = " << event.GetId() << ") don't have assigned LMS parameter." << std::endl;
        return;
    }
    lmsControl->Modify_SPI_Reg_bits(parameter, event.GetInt());
}

void lms7002_pnlRBB_view::OncmbBBLoopbackSelected( wxCommandEvent& event )
{
    switch (cmbBBLoopback->GetSelection())
    {
    case 0:
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFH_RBB, true);
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFL_RBB, false);
        break;
    case 1:
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFH_RBB, false);
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFL_RBB, true);
        break;
    case 2:
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFH_RBB, false);
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFL_RBB, false);
        break;
    default:
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFH_RBB, false);
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFL_RBB, false);
    }
}

void lms7002_pnlRBB_view::UpdateGUI()
{
    assert(lmsControl != nullptr);
    LMS7002_WXGUI::UpdateControlsByMap(this, lmsControl, wndId2Enum);

    long BBloopbackValue = 0;
    long EN_LB_LPFH_RBBvalue = lmsControl->Get_SPI_Reg_bits(EN_LB_LPFH_RBB);

    long EN_LB_LPFL_RBBvalue = lmsControl->Get_SPI_Reg_bits(EN_LB_LPFL_RBB);
    if (!EN_LB_LPFH_RBBvalue && !EN_LB_LPFL_RBBvalue)
        BBloopbackValue = 2;
    else if (EN_LB_LPFH_RBBvalue && !EN_LB_LPFL_RBBvalue)
        BBloopbackValue = 0;
    else if (!EN_LB_LPFH_RBBvalue && EN_LB_LPFL_RBBvalue)
        BBloopbackValue = 1;
    else //invalid combination
    {
        BBloopbackValue = 2;
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFH_RBB, false);
        lmsControl->Modify_SPI_Reg_bits(EN_LB_LPFL_RBB, false);
    }
    cmbBBLoopback->SetSelection(BBloopbackValue);

    //check if B channel is enabled
    if (lmsControl->GetActiveChannel() >= LMS7002M::ChB)
    {
        if (lmsControl->Get_SPI_Reg_bits(MIMO_SISO) != 0)
            wxMessageBox(_("MIMO channel B is disabled"), _("Warning"));
    }
}

void lms7002_pnlRBB_view::OnbtnTuneFilter(wxCommandEvent& event)
{
    double input1;
    double input2;
    txtLowBW_MHz->GetValue().ToDouble(&input1);
    txtHighBW_MHz->GetValue().ToDouble(&input2);
    int status;
    switch (rgrFilterSelection->GetSelection())
    {
    case 0:
        status = lmsControl->TuneRxFilter(LMS7002M::RxFilter::RX_LPF_LOWBAND, input1*1e6);
        break;
    case 1:
        status = lmsControl->TuneRxFilter(LMS7002M::RxFilter::RX_LPF_HIGHBAND, input2*1e6);
        break;
    }
    if (status != 0)
    {
        wxMessageBox(wxString(_("Rx Filter tune: ")) + wxString::From8BitData(GetLastErrorMessage()), _("Error"));
    }
    else switch (rgrFilterSelection->GetSelection())
    {
    case 0:
        {
        wxMessageBox(_("Rx Low band calibration finished"), _("INFO"));
        wxCommandEvent evt;
        evt.SetEventType(LOG_MESSAGE);
        evt.SetString(_("Rx Low band calibrated"));
        wxPostEvent(this, evt);
        break;
        }
    case 1:
        {
        wxMessageBox(_("Rx High band calibration finished"), _("INFO"));
        wxCommandEvent evt;
        evt.SetEventType(LOG_MESSAGE);
        evt.SetString(_("Rx High band calibrated"));
        wxPostEvent(this, evt);
        break;
        }
    }
    lmsControl->DownloadAll();
    UpdateGUI();
}

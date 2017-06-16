#include "lms7002_pnlTBB_view.h"
#include "LMS7002M.h"
#include "ErrorReporting.h"
#include <map>
#include "lms7002_gui_utilities.h"
#include "numericSlider.h"
#include "lms7suiteEvents.h"
using namespace lime;

lms7002_pnlTBB_view::lms7002_pnlTBB_view( wxWindow* parent )
:
pnlTBB_view( parent )
{

}

lms7002_pnlTBB_view::lms7002_pnlTBB_view( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style )
    : pnlTBB_view(parent, id, pos, size, style), lmsControl(nullptr)
{
    wndId2Enum[chkBYPLADDER_TBB] = BYPLADDER_TBB;
    wndId2Enum[cmbCCAL_LPFLAD_TBB] = CCAL_LPFLAD_TBB;
    wndId2Enum[cmbCG_IAMP_TBB] = CG_IAMP_TBB;
    wndId2Enum[chkEN_G_TBB] = EN_G_TBB;
    wndId2Enum[cmbICT_IAMP_FRP_TBB] = ICT_IAMP_FRP_TBB;
    wndId2Enum[cmbICT_IAMP_GG_FRP_TBB] = ICT_IAMP_GG_FRP_TBB;
    wndId2Enum[cmbICT_LPFH_F_TBB] = ICT_LPFH_F_TBB;
    wndId2Enum[cmbICT_LPFLAD_F_TBB] = ICT_LPFLAD_F_TBB;
    wndId2Enum[cmbICT_LPFLAD_PT_TBB] = ICT_LPFLAD_PT_TBB;
    wndId2Enum[cmbICT_LPFS5_F_TBB] = ICT_LPFS5_F_TBB;
    wndId2Enum[cmbICT_LPFS5_PT_TBB] = ICT_LPFS5_PT_TBB;
    wndId2Enum[cmbICT_LPF_H_PT_TBB] = ICT_LPF_H_PT_TBB;
    wndId2Enum[cmbLOOPB_TBB] = LOOPB_TBB;
    wndId2Enum[chkPD_LPFH_TBB] = PD_LPFH_TBB;
    wndId2Enum[chkPD_LPFIAMP_TBB] = PD_LPFIAMP_TBB;
    wndId2Enum[chkPD_LPFLAD_TBB] = PD_LPFLAD_TBB;
    wndId2Enum[chkPD_LPFS5_TBB] = PD_LPFS5_TBB;
    wndId2Enum[cmbRCAL_LPFH_TBB] = RCAL_LPFH_TBB;
    wndId2Enum[cmbRCAL_LPFLAD_TBB] = RCAL_LPFLAD_TBB;
    wndId2Enum[cmbRCAL_LPFS5_TBB] = RCAL_LPFS5_TBB;
    wndId2Enum[cmbTSTIN_TBB] = TSTIN_TBB;
    wndId2Enum[chkEN_DIR_TBB] = EN_DIR_TBB;

    wxArrayString temp;
    temp.clear();
    for (int i = 0; i<32; ++i)
    {
        temp.push_back(wxString::Format(_("%i"), i));
    }
    cmbICT_LPFH_F_TBB->Set(temp);
    cmbICT_LPFLAD_F_TBB->Set(temp);
    cmbICT_LPFLAD_PT_TBB->Set(temp);
    cmbICT_LPFS5_F_TBB->Set(temp);
    cmbICT_LPFS5_PT_TBB->Set(temp);
    cmbICT_LPF_H_PT_TBB->Set(temp);

    temp.clear();
    temp.push_back(_("Disabled"));
    temp.push_back(_("DAC current output"));
    temp.push_back(_("LPFLAD ladder output"));
    temp.push_back(_("TBB output"));
    temp.push_back(_("DAC current output (IQ swap)"));
    temp.push_back(_("LPFLAD ladder output (IQ swap)"));
    temp.push_back(_("TBB output (IQ swap)"));
    cmbLOOPB_TBB->Set(temp);

    temp.clear();
    temp.push_back("Disabled");
    temp.push_back("to Highband filter");
    temp.push_back("to Lowband filter");
    temp.push_back("to current amplifier");
    cmbTSTIN_TBB->Set(temp);

    LMS7002_WXGUI::UpdateTooltips(wndId2Enum, true);
}

void lms7002_pnlTBB_view::Initialize(LMS7002M* pControl)
{
    lmsControl = pControl;
    assert(lmsControl != nullptr);
}

void lms7002_pnlTBB_view::ParameterChangeHandler(wxSpinEvent& event)
{
    wxCommandEvent evt;
    evt.SetInt(event.GetInt());
    evt.SetId(event.GetId());
    evt.SetEventObject(event.GetEventObject());
    ParameterChangeHandler(evt);
}

void lms7002_pnlTBB_view::ParameterChangeHandler(wxCommandEvent& event)
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

void lms7002_pnlTBB_view::UpdateGUI()
{
    LMS7002_WXGUI::UpdateControlsByMap(this, lmsControl, wndId2Enum);
    //check if B channel is enabled
    if (lmsControl->GetActiveChannel() >= LMS7002M::ChB)
    {
        if (lmsControl->Get_SPI_Reg_bits(MIMO_SISO) != 0)
            wxMessageBox(_("MIMO channel B is disabled"), _("Warning"));
    }
}

void lms7002_pnlTBB_view::OnFilterSelectionChange( wxCommandEvent& event )
{
    txtFilterFrequency2->Disable();
	switch (rgrFilterSelection->GetSelection())
	{
	case 0:
		lblFilterInputName->SetLabelText("High BW (MHz)");
		break;
	case 1:
		lblFilterInputName->SetLabelText("Ladder BW (MHz)");
		txtFilterFrequency2->Enable();
		break;
	}
}

void lms7002_pnlTBB_view::OnbtnTuneFilter( wxCommandEvent& event )
{
    double input1;
    double input2;
    txtFilterFrequency->GetValue().ToDouble(&input1);
    txtFilterFrequency2->GetValue().ToDouble(&input2);
    int status;
    switch (rgrFilterSelection->GetSelection())
    {
    case 0:
        status = lmsControl->TuneTxFilter(LMS7002M::TxFilter::TX_HIGHBAND, input1*1e6);
        break;
    case 1:
        status = lmsControl->TuneTxFilterLowBandChain(input1*1e6, input2*1e6);
        break;
    }
    if (status != 0)
    {
        wxMessageBox(wxString(_("Tx Filter tune: ")) + wxString::From8BitData(GetLastErrorMessage()), _("Error"));
    }
    else switch (rgrFilterSelection->GetSelection())
    {
    case 0:
        wxMessageBox(_("Tx High band calibration finished"), _("INFO"));
        break;
    case 1:
        wxMessageBox(_("Tx Low band calibration finished"), _("INFO"));
        break;

    }
    lmsControl->DownloadAll();
    UpdateGUI();
}

void lms7002_pnlTBB_view::OnbtnTuneFilterTest( wxCommandEvent& event )
{
    double input1;
    double input2;
    txtLadderFrequency->GetValue().ToDouble(&input1);
    txtRealpoleFrequency->GetValue().ToDouble(&input2);
    int status;
    switch (rgrFilterSelectionTest->GetSelection())
    {
    case 0:
        status = lmsControl->TuneTxFilter(LMS7002M::TxFilter::TX_LADDER, input1*1e6);
        break;
    case 1:
        status = lmsControl->TuneTxFilter(LMS7002M::TxFilter::TX_REALPOLE, input2*1e6);
        break;
    }
    if (status != 0)
    {
        wxMessageBox(wxString(_("Tx Filter tune: ")) + wxString::From8BitData(GetLastErrorMessage()), _("Error"));
    }
    else switch (rgrFilterSelectionTest->GetSelection())
    {
    case 0:
        {
        wxMessageBox(_("Tx Ladder calibration finished"), _("INFO"));
        wxCommandEvent evt;
        evt.SetEventType(LOG_MESSAGE);
        evt.SetString(_("Tx Ladder calibrated"));
        wxPostEvent(this, evt);
        break;
        }
    case 1:
        {
        wxMessageBox(_("Tx Realpole calibration finished"), _("INFO"));
        wxCommandEvent evt;
        evt.SetEventType(LOG_MESSAGE);
        evt.SetString(_("Tx Realpole calibrated"));
        wxPostEvent(this, evt);
        break;
        }
    }
    lmsControl->DownloadAll();
    UpdateGUI();
}

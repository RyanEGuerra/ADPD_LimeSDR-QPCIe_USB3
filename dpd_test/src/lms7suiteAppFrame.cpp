#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif //WX_PRECOMP

#include "lms7002_mainPanel.h"

#include "lms7suiteAppFrame.h"
#include "LMS7002M.h"
#include "IConnection.h"
#include "dlgAbout.h"
#include "dlgConnectionSettings.h"
#include "lms7suiteEvents.h"
#include "fftviewer_frFFTviewer.h"
#include "LMS_StreamBoard.h"
#include "ADF4002.h"
#include "ADF4002_wxgui.h"
#include "Si5351C.h"
#include "Si5351C_wxgui.h"
#include "LMS_Programing_wxgui.h"
#include "pnlMiniLog.h"
#include "RFSpark_wxgui.h"
#include "HPM7_wxgui.h"
#include "FPGAcontrols_wxgui.h"
#include "myriad7_wxgui.h"
#include "lms7002m_novena_wxgui.h"
#include "SPI_wxgui.h"
#include <wx/string.h>
#include "dlgDeviceInfo.h"
#include <functional>
#include "lms7002_pnlTRF_view.h"
#include "lms7002_pnlRFE_view.h"
#include "pnlBoardControls.h"
#include <ConnectionRegistry.h>
#include <LMSBoards.h>
#include "DPDTest/DPDTest.h"
#include "pnlQSpark.h"

using namespace std;
using namespace lime;

///////////////////////////////////////////////////////////////////////////

const wxString LMS7SuiteAppFrame::cWindowTitle = _("LMS7Suite");

void LMS7SuiteAppFrame::HandleLMSevent(wxCommandEvent& event)
{
    if (event.GetEventType() == CGEN_FREQUENCY_CHANGED)
    {
        int status = lmsControl->SetInterfaceFrequency(lmsControl->GetFrequencyCGEN(), lmsControl->Get_SPI_Reg_bits(HBI_OVR_TXTSP), lmsControl->Get_SPI_Reg_bits(HBD_OVR_RXTSP));
        if (status == 0)
        {
            wxCommandEvent evt;
            evt.SetEventType(LOG_MESSAGE);
            wxString msg;
            msg += _("Parameters modified: ");
            msg += wxString::Format(_("HBI_OVR: %i "), lmsControl->Get_SPI_Reg_bits(HBI_OVR_TXTSP, false));
            msg += wxString::Format(_("TXTSPCLKA_DIV: %i "), lmsControl->Get_SPI_Reg_bits(TXTSPCLKA_DIV, false));
            msg += wxString::Format(_("TXDIVEN: %i "), lmsControl->Get_SPI_Reg_bits(TXDIVEN, false));
            msg += wxString::Format(_("MCLK1SRC: %i "), lmsControl->Get_SPI_Reg_bits(MCLK1SRC, false));
            msg += wxString::Format(_("HBD_OVR: %i "), lmsControl->Get_SPI_Reg_bits(HBD_OVR_RXTSP, false));
            msg += wxString::Format(_("RXTSPCLKA_DIV: %i "), lmsControl->Get_SPI_Reg_bits(RXTSPCLKA_DIV, false));
            msg += wxString::Format(_("RXDIVEN: %i "), lmsControl->Get_SPI_Reg_bits(RXDIVEN, false));
            msg += wxString::Format(_("MCLK2SRC: %i "), lmsControl->Get_SPI_Reg_bits(MCLK2SRC, false));
            evt.SetString(msg);
            wxPostEvent(this, evt);
        }
        if (streamBoardPort && streamBoardPort->IsOpen() && streamBoardPort->GetDeviceInfo().deviceName != GetDeviceName(LMS_DEV_NOVENA))
        {
            //if decimation/interpolation is 0(2^1) or 7(bypass), interface clocks should not be divided
            int decimation = lmsControl->Get_SPI_Reg_bits(HBD_OVR_RXTSP);
            float interfaceRx_Hz = lmsControl->GetReferenceClk_TSP(LMS7002M::Rx);
            if (decimation != 7)
                interfaceRx_Hz /= pow(2.0, decimation);
            int interpolation = lmsControl->Get_SPI_Reg_bits(HBI_OVR_TXTSP);
            float interfaceTx_Hz = lmsControl->GetReferenceClk_TSP(LMS7002M::Tx);
            if (interpolation != 7)
                interfaceTx_Hz /= pow(2.0, interpolation);
            const int channelsCount = 2;
            streamBoardPort->UpdateExternalDataRate(0, interfaceTx_Hz/channelsCount, interfaceRx_Hz/channelsCount);
            if (status != LMS_StreamBoard::SUCCESS)
                wxMessageBox(_("Failed to configure Stream board PLL"), _("Warning"));
            else
            {
                wxCommandEvent evt;
                evt.SetEventType(LOG_MESSAGE);
                evt.SetString(wxString::Format(_("Stream board PLL configured Tx: %.3f MHz Rx: %.3f MHz Angle: %.0f deg"), interfaceTx_Hz/1e6, interfaceRx_Hz/1e6, 90.0));
                wxPostEvent(this, evt);
            }
        }
        if (fftviewer)
        {
            int decimation = lmsControl->Get_SPI_Reg_bits(HBD_OVR_RXTSP);
            float samplingFreq_Hz = lmsControl->GetReferenceClk_TSP(LMS7002M::Rx);
            if (decimation != 7)
                samplingFreq_Hz /= pow(2.0, decimation+1);
            fftviewer->SetNyquistFrequency(samplingFreq_Hz / 2);
        }
        if(DPDTestGui)
        {
            int decimation = lmsControl->Get_SPI_Reg_bits(HBD_OVR_RXTSP);
            float samplingFreq_MHz = lmsControl->GetReferenceClk_TSP(LMS7002M::Rx);
            samplingFreq_MHz /= pow(2.0, decimation + 1);
            DPDTestGui->SetNyquist(samplingFreq_MHz / 2);
        }
    }

    //in case of Novena board, need to update GPIO
    if (lms7controlPort && lms7controlPort->GetDeviceInfo().deviceName != GetDeviceName(LMS_DEV_NOVENA) &&
        (event.GetEventType() == LMS7_TXBAND_CHANGED || event.GetEventType() == LMS7_RXPATH_CHANGED))
    {
        //update external band-selection to match
        lmsControl->UpdateExternalBandSelect();
        if (novenaGui)
            novenaGui->UpdatePanel();
    }

    if (event.GetEventType() == LMS7_TXBAND_CHANGED)
    {
        const wxObject* eventSource = event.GetEventObject();
        const int bandIndex = event.GetInt();
        //update HPM7 if changes were made outside of it
        if (lms7controlPort && lms7controlPort->GetDeviceInfo().expansionName == GetExpansionBoardName(EXP_BOARD_HPM7) && eventSource != hpm7)
            hpm7->SelectBand(bandIndex);
        if (lms7controlPort && eventSource == hpm7)
        {
            lmsControl->Modify_SPI_Reg_bits(SEL_BAND1_TRF, bandIndex == 0);
            lmsControl->Modify_SPI_Reg_bits(SEL_BAND2_TRF, bandIndex == 1);
            mContent->mTabTRF->UpdateGUI();
        }
    }
    if (event.GetEventType() == LMS7_RXPATH_CHANGED)
    {
        const wxObject* eventSource = event.GetEventObject();
        const int pathIndex = event.GetInt();
        //update HPM7 if changes were made outside of it
        if (lms7controlPort && lms7controlPort->GetDeviceInfo().expansionName == GetExpansionBoardName(EXP_BOARD_HPM7) && eventSource != hpm7)
            hpm7->SelectRxPath(pathIndex);
        if (lms7controlPort && eventSource == hpm7)
        {
            lmsControl->Modify_SPI_Reg_bits(SEL_PATH_RFE, pathIndex);
            mContent->mTabRFE->UpdateGUI();
        }
    }
}

LMS7SuiteAppFrame::LMS7SuiteAppFrame( wxWindow* parent ) :
    AppFrame_view( parent ), lms7controlPort(nullptr), streamBoardPort(nullptr)
{
#ifndef __unix__
    SetIcon(wxIcon(_("aaaaAPPicon")));
#endif
    programmer = nullptr;
    fftviewer = nullptr;
    adfGUI = nullptr;
    si5351gui = nullptr;
    rfspark = nullptr;
    hpm7 = nullptr;
    fpgaControls = nullptr;
    myriad7 = nullptr;
    deviceInfo = nullptr;
    spi = nullptr;
    novenaGui = nullptr;
    boardControlsGui = nullptr;
    DPDTestGui = nullptr;
    qSparkGui = nullptr;

    lmsControl = new LMS7002M();
    mContent->Initialize(lmsControl);
    Connect(CGEN_FREQUENCY_CHANGED, wxCommandEventHandler(LMS7SuiteAppFrame::HandleLMSevent), NULL, this);
    Connect(LMS7_TXBAND_CHANGED, wxCommandEventHandler(LMS7SuiteAppFrame::HandleLMSevent), NULL, this);
    Connect(LMS7_RXPATH_CHANGED, wxCommandEventHandler(LMS7SuiteAppFrame::HandleLMSevent), NULL, this);
    mMiniLog = new pnlMiniLog(this, wxNewId());
    Connect(LOG_MESSAGE, wxCommandEventHandler(LMS7SuiteAppFrame::OnLogMessage), 0, this);

    contentSizer->Add(mMiniLog, 1, wxEXPAND, 5);

    adfModule = new ADF4002();
    si5351module = new Si5351C();

	Layout();
	Fit();

    SetMinSize(GetSize());
    UpdateConnections(lms7controlPort, streamBoardPort);

    mnuCacheValues->Check(lmsControl->IsValuesCacheEnabled());
}

LMS7SuiteAppFrame::~LMS7SuiteAppFrame()
{
    Disconnect(CGEN_FREQUENCY_CHANGED, wxCommandEventHandler(LMS7SuiteAppFrame::HandleLMSevent), NULL, this);
    delete lmsControl;
    ConnectionRegistry::freeConnection(lms7controlPort);
    ConnectionRegistry::freeConnection(streamBoardPort);
}

void LMS7SuiteAppFrame::OnClose( wxCloseEvent& event )
{
    Destroy();
}

void LMS7SuiteAppFrame::OnQuit( wxCommandEvent& event )
{
    Destroy();
}

void LMS7SuiteAppFrame::OnShowConnectionSettings( wxCommandEvent& event )
{
	dlgConnectionSettings dlg(this);

    if (fftviewer)
        fftviewer->StopStreaming();

    dlg.SetConnectionManagers(&lms7controlPort, &streamBoardPort);
    Bind(CONTROL_PORT_CONNECTED, wxCommandEventHandler(LMS7SuiteAppFrame::OnControlBoardConnect), this);
    Bind(DATA_PORT_CONNECTED, wxCommandEventHandler(LMS7SuiteAppFrame::OnDataBoardConnect), this);
    Bind(CONTROL_PORT_DISCONNECTED, wxCommandEventHandler(LMS7SuiteAppFrame::OnControlBoardConnect), this);
    Bind(DATA_PORT_DISCONNECTED, wxCommandEventHandler(LMS7SuiteAppFrame::OnDataBoardConnect), this);
	dlg.ShowModal();
}

void LMS7SuiteAppFrame::OnAbout( wxCommandEvent& event )
{
	dlgAbout dlg(this);
    dlg.ShowModal();
}


void LMS7SuiteAppFrame::OnControlBoardConnect(wxCommandEvent& event)
{
    UpdateConnections(lms7controlPort, streamBoardPort);
    const int controlCollumn = 1;
    if (lms7controlPort && lms7controlPort->IsOpen())
    {
        //bind callback for spi data logging
        lms7controlPort->SetDataLogCallback(bind(&LMS7SuiteAppFrame::OnLogDataTransfer, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        DeviceInfo info = lms7controlPort->GetDeviceInfo();
        wxString controlDev = _("Control port: ");
        controlDev.Append(info.deviceName);
        controlDev.Append(wxString::Format(_(" FW:%s HW:%s Protocol:%s"), info.firmwareVersion, info.hardwareVersion, info.protocolVersion));
        statusBar->SetStatusText(controlDev, controlCollumn);

        wxCommandEvent evt;
        evt.SetEventType(LOG_MESSAGE);
        evt.SetString(_("Connected ") + controlDev);
        wxPostEvent(this, evt);
        if (si5351gui)
            si5351gui->ModifyClocksGUI(info.deviceName);
        if (boardControlsGui)
            boardControlsGui->SetupControls(info.deviceName);
    }
    else
    {
        statusBar->SetStatusText(_("Control port: Not Connected"), controlCollumn);
        wxCommandEvent evt;
        evt.SetEventType(LOG_MESSAGE);
        evt.SetString(_("Disconnected control port"));
        wxPostEvent(this, evt);
    }
}

void LMS7SuiteAppFrame::OnDataBoardConnect(wxCommandEvent& event)
{
    UpdateConnections(lms7controlPort, streamBoardPort);
    const int dataCollumn = 2;
    if (streamBoardPort && streamBoardPort->IsOpen())
    {
        //bind callback for spi data logging
        streamBoardPort->SetDataLogCallback(bind(&LMS7SuiteAppFrame::OnLogDataTransfer, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        DeviceInfo info = streamBoardPort->GetDeviceInfo();
        wxString controlDev = _("Data port: ");
        controlDev.Append(info.deviceName);
        controlDev.Append(wxString::Format(_(" FW:%s HW:%s Protocol:%s"), info.firmwareVersion, info.hardwareVersion, info.protocolVersion));
        statusBar->SetStatusText(controlDev, dataCollumn);

        if(DPDTestGui)
            DPDTestGui->Initialize(streamBoardPort);

        wxCommandEvent evt;
        evt.SetEventType(LOG_MESSAGE);
        evt.SetString(_("Connected ") + controlDev);
        wxPostEvent(this, evt);
    }
    else
    {
        statusBar->SetStatusText(_("Data port: Not Connected"), dataCollumn);
        wxCommandEvent evt;
        evt.SetEventType(LOG_MESSAGE);
        evt.SetString(_("Disconnected data port"));
        wxPostEvent(this, evt);
    }
}

void LMS7SuiteAppFrame::OnFFTviewerClose(wxCloseEvent& event)
{
    fftviewer->StopStreaming();
    fftviewer->Destroy();
    fftviewer = nullptr;
}

void LMS7SuiteAppFrame::OnShowFFTviewer(wxCommandEvent& event)
{
    if (fftviewer) //it's already opened
        fftviewer->Show();
    else
    {
        fftviewer = new fftviewer_frFFTviewer(this);
        fftviewer->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnFFTviewerClose), NULL, this);
        fftviewer->Show();
        int decimation = lmsControl->Get_SPI_Reg_bits(HBD_OVR_RXTSP);
        float samplingFreq_Hz = lmsControl->GetReferenceClk_TSP(LMS7002M::Rx);
        if (decimation != 7)
            samplingFreq_Hz /= pow(2.0, decimation+1);
        fftviewer->SetNyquistFrequency(samplingFreq_Hz / 2);
    }
    fftviewer->Initialize(streamBoardPort);
}

void LMS7SuiteAppFrame::OnADF4002Close(wxCloseEvent& event)
{
    adfGUI->Destroy();
    adfGUI = nullptr;
}

void LMS7SuiteAppFrame::OnShowADF4002(wxCommandEvent& event)
{
    if (adfGUI) //it's already opened
        adfGUI->Show();
    else
    {
        adfGUI = new ADF4002_wxgui(this, wxNewId(), _("ADF4002"));
        adfGUI->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnADF4002Close), NULL, this);
        adfGUI->Initialize(adfModule, lms7controlPort);
        adfGUI->Show();
    }
}

void LMS7SuiteAppFrame::OnSi5351Close(wxCloseEvent& event)
{
    si5351gui->Destroy();
    si5351gui = nullptr;
}

void LMS7SuiteAppFrame::OnShowSi5351C(wxCommandEvent& event)
{
    if (si5351gui) //it's already opened
        si5351gui->Show();
    else
    {
        si5351gui = new Si5351C_wxgui(this, wxNewId(), _("Si5351C"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE);
        si5351gui->Initialize(si5351module);
// TODO : modify clock names according to connected board
//        si5351gui->ModifyClocksGUI(lms7controlPort->GetInfo().device);
        si5351gui->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnSi5351Close), NULL, this);
        si5351gui->Show();
    }
}

void LMS7SuiteAppFrame::OnProgramingClose(wxCloseEvent& event)
{
    programmer->Destroy();
    programmer = nullptr;
}

void LMS7SuiteAppFrame::OnShowPrograming(wxCommandEvent& event)
{
    if (programmer) //it's already opened
        programmer->Show();
    else
    {
        programmer = new LMS_Programing_wxgui(this, wxNewId(), _("Programing"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE);
        programmer->SetConnection(lms7controlPort);
        programmer->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnProgramingClose), NULL, this);
        programmer->Show();
    }
}

void LMS7SuiteAppFrame::OnLogMessage(wxCommandEvent &event)
{
    if (mMiniLog)
        mMiniLog->HandleMessage(event);
}

void LMS7SuiteAppFrame::OnRFSparkClose(wxCloseEvent& event)
{
    rfspark->Destroy();
    rfspark = nullptr;
}
void LMS7SuiteAppFrame::OnShowRFSpark(wxCommandEvent& event)
{
    if (rfspark) //it's already opened
        rfspark->Show();
    else
    {
        rfspark = new RFSpark_wxgui(this, wxNewId(), _("RF-ESpark"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE);
        rfspark->Initialize(lms7controlPort);
        rfspark->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnRFSparkClose), NULL, this);
        rfspark->Show();
    }
}

void LMS7SuiteAppFrame::OnHPM7Close(wxCloseEvent& event)
{
    hpm7->Destroy();
    hpm7 = nullptr;
}
void LMS7SuiteAppFrame::OnShowHPM7(wxCommandEvent& event)
{
    if (hpm7) //it's already opened
        hpm7->Show();
    else
    {
        hpm7 = new HPM7_wxgui(this, wxNewId(), _("HPM7"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE);
        hpm7->Initialize(lms7controlPort);
        hpm7->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnHPM7Close), NULL, this);
        hpm7->Show();
    }
}

void LMS7SuiteAppFrame::OnFPGAcontrolsClose(wxCloseEvent& event)
{
    fpgaControls->Destroy();
    fpgaControls = nullptr;
}
void LMS7SuiteAppFrame::OnShowFPGAcontrols(wxCommandEvent& event)
{
    if (fpgaControls) //it's already opened
        fpgaControls->Show();
    else
    {
        fpgaControls = new FPGAcontrols_wxgui(this, wxNewId(), _("FPGA Controls"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE);
        fpgaControls->Initialize(streamBoardPort);
        fpgaControls->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnFPGAcontrolsClose), NULL, this);
        fpgaControls->Show();
    }
}

void LMS7SuiteAppFrame::OnMyriad7Close(wxCloseEvent& event)
{
    myriad7->Destroy();
    myriad7 = nullptr;
}
void LMS7SuiteAppFrame::OnShowMyriad7(wxCommandEvent& event)
{
    if (myriad7) //it's already opened
        myriad7->Show();
    else
    {
        myriad7 = new Myriad7_wxgui(this, wxNewId(), _("Myriad7"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE);
        myriad7->Initialize(lms7controlPort);
        myriad7->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnMyriad7Close), NULL, this);
        myriad7->Show();
    }
}

void LMS7SuiteAppFrame::OnDeviceInfoClose(wxCloseEvent& event)
{
    deviceInfo->Destroy();
    deviceInfo = nullptr;
}

void LMS7SuiteAppFrame::OnShowDeviceInfo(wxCommandEvent& event)
{
    if (deviceInfo) //it's already opened
        deviceInfo->Show();
    else
    {
        deviceInfo = new dlgDeviceInfo(this, wxNewId(), _("Device Info"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        deviceInfo->Initialize(lms7controlPort, streamBoardPort);
        deviceInfo->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnDeviceInfoClose), NULL, this);
        deviceInfo->Show();
    }
}

void LMS7SuiteAppFrame::OnSPIClose(wxCloseEvent& event)
{
    spi->Destroy();
    spi = nullptr;
}

void LMS7SuiteAppFrame::OnShowSPI(wxCommandEvent& event)
{
    if (spi) //it's already opened
        spi->Show();
    else
    {
        spi = new SPI_wxgui(this, wxNewId(), _("Device Info"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        spi->Initialize(lms7controlPort, streamBoardPort);
        spi->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnSPIClose), NULL, this);
        spi->Show();
    }
}

#include <iomanip>
void LMS7SuiteAppFrame::OnLogDataTransfer(bool Tx, const unsigned char* data, const unsigned int length)
{
    if (mMiniLog == nullptr || mMiniLog->chkLogData->IsChecked() == false)
        return;
    stringstream ss;
    ss << (Tx ? "Wr(" : "Rd(");
    ss << length << "): ";
    ss << std::hex << std::setfill('0');
    int repeatedZeros = 0;
    for (int i = length - 1; i >= 0; --i)
        if (data[i] == 0)
            ++repeatedZeros;
        else
            break;
    if (repeatedZeros == 2)
        repeatedZeros = 0;
    repeatedZeros = repeatedZeros - (repeatedZeros & 0x1);
    for (int i = 0; i<length - repeatedZeros; ++i)
        //casting to short to print as numbers
        ss << " " << std::setw(2) << (unsigned short)data[i];
    if (repeatedZeros > 2)
        ss << " (00 x " << std::dec << repeatedZeros << " times)";
    cout << ss.str() << endl;
    wxCommandEvent *evt = new wxCommandEvent();
    evt->SetString(ss.str());
    evt->SetEventObject(this);
    evt->SetEventType(LOG_MESSAGE);
    wxQueueEvent(this, evt);
}

void LMS7SuiteAppFrame::OnShowNovena(wxCommandEvent& event)
{
    if (novenaGui) //it's already opened
        novenaGui->Show();
    else
    {
        novenaGui = new LMS7002M_Novena_wxgui(this, wxNewId(), _("Novena"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        novenaGui->Initialize(lms7controlPort);
        novenaGui->UpdatePanel();
        novenaGui->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnNovenaClose), NULL, this);
        novenaGui->Show();
    }
}

void LMS7SuiteAppFrame::OnNovenaClose(wxCloseEvent& event)
{
    novenaGui->Destroy();
    novenaGui = nullptr;
}

void LMS7SuiteAppFrame::OnShowBoardControls(wxCommandEvent& event)
{
    if (boardControlsGui) //it's already opened
        boardControlsGui->Show();
    else
    {
        boardControlsGui = new pnlBoardControls(this, wxNewId(), _("Board related controls"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
        boardControlsGui->Initialize(lms7controlPort);
        boardControlsGui->UpdatePanel();
        boardControlsGui->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnBoardControlsClose), NULL, this);
        boardControlsGui->Show();
    }
}

void LMS7SuiteAppFrame::OnBoardControlsClose(wxCloseEvent& event)
{
    boardControlsGui->Destroy();
    boardControlsGui = nullptr;
}

void LMS7SuiteAppFrame::UpdateConnections(IConnection* lms7controlPort, IConnection* streamBoardPort)
{
    if(lmsControl)
        lmsControl->SetConnection(lms7controlPort);
    if(si5351module)
        si5351module->Initialize(lms7controlPort);
    if(fftviewer)
        fftviewer->Initialize(streamBoardPort);
    if(adfGUI)
        adfGUI->Initialize(adfModule, lms7controlPort);
    if(rfspark)
        rfspark->Initialize(lms7controlPort);
    if(hpm7)
        hpm7->Initialize(lms7controlPort);
    if(fpgaControls)
        fpgaControls->Initialize(streamBoardPort);
    if(myriad7)
        myriad7->Initialize(lms7controlPort);
    if(deviceInfo)
        deviceInfo->Initialize(lms7controlPort, streamBoardPort);
    if(spi)
        spi->Initialize(lms7controlPort, streamBoardPort);
    if(novenaGui)
        novenaGui->Initialize(lms7controlPort);
    if(boardControlsGui)
        boardControlsGui->Initialize(lms7controlPort);
    if(programmer)
        programmer->SetConnection(lms7controlPort);
    if(qSparkGui)
        qSparkGui->Initialize(lms7controlPort);

	if (DPDTestGui)
		DPDTestGui->Initialize(streamBoardPort);
	
}

void LMS7SuiteAppFrame::OnChangeCacheSettings(wxCommandEvent& event)
{
    int checked = event.GetInt();
    lmsControl->EnableValuesCache(checked);
}

void LMS7SuiteAppFrame::OnDPDTestClose(wxCloseEvent& event)
{
    DPDTestGui->Destroy();
    DPDTestGui = nullptr;
}

void LMS7SuiteAppFrame::SetDPDNyquist(double freq)
{
	if (DPDTestGui) //it's already opened
	{

		DPDTestGui->Show();
		DPDTestGui->SetNyquist(freq);
		//wxMessageBox(_("Error"), _("Error"), wxOK);
		///int checked = 9999;
	}
}

void LMS7SuiteAppFrame::OnShowDPDTest(wxCommandEvent& event)
{
    if(DPDTestGui) //it's already opened
        DPDTestGui->Show();
    else
    {
		DPDTestGui = new DPDTest(this, wxNewId(), _("DPDTest"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxRESIZE_BORDER);
        DPDTestGui->Initialize(streamBoardPort);
        int decimation = lmsControl->Get_SPI_Reg_bits(HBD_OVR_RXTSP);
        float samplingFreq = lmsControl->GetReferenceClk_TSP(LMS7002M::Rx);
        if(decimation != 7)
            samplingFreq /= pow(2.0, decimation + 1);


        DPDTestGui->SetNyquist(samplingFreq / 2);
        
		
		DPDTestGui->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnDPDTestClose), NULL, this);
        DPDTestGui->Show();
    }
}

void LMS7SuiteAppFrame::OnShowQSpark(wxCommandEvent& event)
{
    if(qSparkGui) //it's already opened
        qSparkGui->Show();
    else
    {
        qSparkGui = new pnlQSpark(this, wxNewId(), _("QSpark controls"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
        qSparkGui->Initialize(lms7controlPort);
        qSparkGui->UpdatePanel();
        qSparkGui->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LMS7SuiteAppFrame::OnQSparkClose), NULL, this);
        qSparkGui->Show();
    }
}

void LMS7SuiteAppFrame::OnQSparkClose(wxCloseEvent& event)
{
    qSparkGui->Destroy();
    qSparkGui = nullptr;
}

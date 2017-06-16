
#include "ErrorReporting.h"
#include "lms7002_pnlBuffers_view.h"
#include "LMS64CProtocol.h"
#include "wx/msgdlg.h"
using namespace lime;

static unsigned char setbit(const unsigned char src, const int pos, const bool value)
{
    int val = src;
    val = val & ~(0x1 << pos);
    val |= value << pos;
    return val;
}
static bool getbit(const unsigned char src, const int pos)
{
    return (src >> pos) & 0x01;
}

lms7002_pnlBuffers_view::lms7002_pnlBuffers_view( wxWindow* parent )
:
pnlBuffers_view(parent), serPort(nullptr)
{
}

lms7002_pnlBuffers_view::lms7002_pnlBuffers_view(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : pnlBuffers_view(parent, id, pos, size, style), serPort(nullptr)
{   
}

void lms7002_pnlBuffers_view::OnGPIOchanged( wxCommandEvent& event )
{
    uint8_t value = 0;
    value = setbit(value, 0, chkDIO_DIR_CTRL1->GetValue());
    value = setbit(value, 1, chkDIO_DIR_CTRL2->GetValue());
    value = setbit(value, 2, chkDIO_BUFF_OE->GetValue());
    value = setbit(value, 3, chkIQ_SEL1_DIR->GetValue());
    value = setbit(value, 4, chkIQ_SEL2_DIR->GetValue());
    value = setbit(value, 5, chkG_PWR_DWN->GetValue());
    LMS64CProtocol::GenericPacket pkt;
    pkt.cmd = CMD_GPIO_WR;
    pkt.outBuffer.push_back(value);
    if (serPort->TransferPacket(pkt) != 0)
    {
        wxMessageBox(wxString::Format(_("GPIO write: %s"), wxString::From8BitData(GetLastErrorMessage())));
    }
}

void lms7002_pnlBuffers_view::UpdateGUI()
{
    if (serPort == nullptr)
        return;
    LMS64CProtocol::GenericPacket pkt;
    pkt.cmd = CMD_GPIO_RD;
    if (serPort->TransferPacket(pkt) == 0)
    {   
        chkDIO_BUFF_OE->SetValue(getbit(pkt.inBuffer[0], 2));
        chkDIO_DIR_CTRL1->SetValue(getbit(pkt.inBuffer[0], 0));
        chkDIO_DIR_CTRL2->SetValue(getbit(pkt.inBuffer[0], 1));
        chkIQ_SEL1_DIR->SetValue(getbit(pkt.inBuffer[0], 3));
        chkIQ_SEL2_DIR->SetValue(getbit(pkt.inBuffer[0], 4));
        chkG_PWR_DWN->SetValue(getbit(pkt.inBuffer[0], 5));
    }
    else wxMessageBox(wxString::Format(_("GPIO read: %s"), wxString::From8BitData(GetLastErrorMessage())));
    Refresh();
}

void lms7002_pnlBuffers_view::Initialize(IConnection* pSerPort)
{
    serPort = dynamic_cast<LMS64CProtocol *>(pSerPort);
}

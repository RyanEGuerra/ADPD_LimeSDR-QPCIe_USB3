///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Jun 17 2015)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __FFTVIEWER_WXGUI_H__
#define __FFTVIEWER_WXGUI_H__

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
class OpenGLGraph;

#include <wx/panel.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/splitter.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/valtext.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/gauge.h>
#include <wx/frame.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class frFFTviewer
///////////////////////////////////////////////////////////////////////////////
class frFFTviewer : public wxFrame 
{
	private:
	
	protected:
		wxSplitterWindow* mPlotsSplitter;
		wxPanel* mTimeConstellationPanel;
		wxSplitterWindow* m_splitter3;
		OpenGLGraph* mTimeDomainPanel;
		OpenGLGraph* mConstelationPanel;
		OpenGLGraph* mFFTpanel;
		wxStaticText* m_staticText6;
		wxTextCtrl* txtNyquistFreqMHz;
		wxStaticText* m_staticText7;
		wxSpinCtrl* spinFFTsize;
		wxChoice* cmbStreamType;
		wxButton* btnStartStop;
		wxStaticText* m_staticText8;
		wxStaticText* lblRxDataRate;
		wxStaticText* m_staticText18;
		wxStaticText* lblTxDataRate;
		wxStaticText* m_staticText10;
		wxStaticText* lblFFTrate;
		wxCheckBox* chkFreezeTimeDomain;
		wxCheckBox* chkFreezeConstellation;
		wxCheckBox* chkFreezeFFT;
		wxStaticText* m_staticText11;
		wxChoice* cmbChannelVisibility;
		wxStaticText* m_staticText15;
		wxGauge* gaugeRxBuffer;
		wxStaticText* m_staticText16;
		wxGauge* gaugeTxBuffer;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnFFTsamplesCountChanged( wxSpinEvent& event ) { event.Skip(); }
		virtual void OnbtnStartStop( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnChannelVisibilityChange( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		frFFTviewer( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("FFT viewer"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );
		
		~frFFTviewer();
		
		void mPlotsSplitterOnIdle( wxIdleEvent& )
		{
			mPlotsSplitter->SetSashPosition( 0 );
			mPlotsSplitter->Disconnect( wxEVT_IDLE, wxIdleEventHandler( frFFTviewer::mPlotsSplitterOnIdle ), NULL, this );
		}
		
		void m_splitter3OnIdle( wxIdleEvent& )
		{
			m_splitter3->SetSashPosition( 0 );
			m_splitter3->Disconnect( wxEVT_IDLE, wxIdleEventHandler( frFFTviewer::m_splitter3OnIdle ), NULL, this );
		}
	
};

#endif //__FFTVIEWER_WXGUI_H__

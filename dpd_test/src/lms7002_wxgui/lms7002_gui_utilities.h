/**
@file 	gui_utilities.h
@author Lime Microsystems (www.limemicro.com)
@brief 	Common functions used by all panels
*/
#include <map>
#include "LMS7002M_parameters.h"
#include <vector>
class wxPanel;
class wxWindow;

namespace lime{
class LMS7002M;
}

namespace LMS7002_WXGUI
{

void UpdateControlsByMap(wxPanel* panel, lime::LMS7002M* lmsControl, const std::map<wxWindow*, lime::LMS7Parameter> &wndId2param);

typedef std::pair<int, int> indexValuePair;
typedef std::vector < indexValuePair > indexValueMap;

int index2value(int index, const indexValueMap &pairs);
int value2index(int value, const indexValueMap &pairs);

void UpdateTooltips(const std::map<wxWindow*, lime::LMS7Parameter> &wndId2param, bool replace);

}

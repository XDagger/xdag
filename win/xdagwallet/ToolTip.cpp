/***************************************************************************
This software will show a squared tooltip.
OVERFLOWS CHECKING WILL NOT BE STRESSED
since the applications are not critical.

Copyright (C) 2018  Marco Scarlino <marco.scarlino@students-live.uniroma2.it>.

ToolTip (consisting of ToolTip.cpp and ToolTip.h) is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************/

#include "ToolTip.h"

BEGIN_MESSAGE_MAP(CToolTip, CFrameWnd)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_TIMER()
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_ACTIVATEAPP()
END_MESSAGE_MAP()

// Constructor
CToolTip::CToolTip()
{
	_rgnRect.CreateRectRgn(0,0,0,0);
}

//Destructor
CToolTip::~CToolTip()
{
}

int CToolTip::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
	{
		return -1;
	}
	
	ModifyStyle(WS_CAPTION, 0);

	LONG rectWidth = _rect.right;
	LONG rectHeight = _rect.bottom;

	_rectText.left = 0;
	_rectText.right = rectWidth;
	_rectText.top = 0;
	_rectText.bottom = rectHeight;

	/****!!	 Possible overflow	!!****/
	_rgnRect.SetRectRgn(0, 0, (int)rectWidth, (int)rectHeight);
	/*********************************/

	return 0;
}

void CToolTip::OnPaint() 
{
	CPaintDC dc(this); 

	CBrush brOutlineBrush;
    	brOutlineBrush.CreateSolidBrush(RGB(0, 0, 0)); 
   
    	CBrush brFillBrush;
	COLORREF crBackground = ::GetSysColor(COLOR_INFOBK);
    	brFillBrush.CreateSolidBrush(crBackground);

    	dc.FillRgn(&_rgnRect, &brFillBrush);
    	dc.FrameRgn(&_rgnRect, &brOutlineBrush, 1, 1);

   	int nBkMode = dc.SetBkMode(TRANSPARENT);
    	COLORREF clrPrevious =  dc.SetTextColor(RGB(0, 0, 0));

    	dc.DrawText(_strMessage, _rectText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    	dc.SetBkColor(nBkMode);
    	dc.SetTextColor(clrPrevious);
	
}

BOOL CToolTip::PreCreateWindow(CREATESTRUCT& cs)
{
	// Override the creation of the new default window with an invisible one
	// and join this one with the already existant.
	
	if (!CWnd::PreCreateWindow(cs))
    {
		return FALSE;
    }
	
 	if (!::IsWindow(m_wndInvisibleParent.m_hWnd))
 	{
        PCSTR pstrOwnerClass = ::AfxRegisterWndClass(0);
       
        BOOL bError = m_wndInvisibleParent.CreateEx(0,pstrOwnerClass,  _T(""), WS_POPUP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, 0);

        if (bError == FALSE)
        {
			return FALSE;
        }
		
 	}
	cs.hwndParent = m_wndInvisibleParent.m_hWnd;
	
	return TRUE;
}

BOOL CToolTip::Create()
{
	BOOL bResult = TRUE;
	if (GetSafeHwnd() == NULL) {
		PCSTR pstrOwnerClass = ::AfxRegisterWndClass(0);
		bResult = CFrameWnd::Create(pstrOwnerClass, NULL, WS_OVERLAPPED, _rect);
	}

	int captionBarSize = ::GetSystemMetrics(SM_CYCAPTION);
	int verticalBorderSize = ::GetSystemMetrics(SM_CYSIZEFRAME);
	
	SetWindowPos(
		&wndTopMost,
		_rect.left,
		(_rect.top -captionBarSize -verticalBorderSize),
		_rect.right,	//Width
		_rect.bottom,	//Height
		SWP_SHOWWINDOW | SWP_NOACTIVATE
	);
	
    return bResult;
}


int CToolTip::CalculateRectSizeAndPosition(CPoint pt, int charWidth, int charHeight)
{
	int rectLeft, rectWidth, rectTop, rectHeight;

	/****!!	 Possible overflow	!!****/
	int textLength = _strMessage.GetLength() * charWidth;
	int height = (int)(charHeight*1.1);
	rectLeft = (int)pt.x;
	rectWidth = (int)(textLength * 1.1);
	rectTop = (int)pt.y;
	/*********************************/

	rectHeight = height;
	_rect=CRect(rectLeft, rectTop, rectWidth, rectHeight);
	return 0;
}

//Entry Point
int CToolTip::Show(CPoint pt, LPRECT lpRect,
	int charWidth, int charHeight, CString strMessage, UINT secs)
{
	if (secs <= 0)
	{
		fprintf(stderr, "ToolTip timer need nSec > 0\n");
		return -1;
	}

	_strMessage = strMessage;
	CalculateRectSizeAndPosition(pt, charWidth, charHeight);

	if (Create() != TRUE) 
	{
		fprintf(stderr, "Cannot create the frame window associated with the CFrameWnd\n");
		return -1;
	}

	if (SetTimer(ID_TIMER_POPUP, (secs * 1000), NULL) == NULL)
	{
		fprintf(stderr, "Cannot set the timer to hide the tooltip\n");
		return -1;
	}

	return 0;
}

void CToolTip::OnRButtonDown(UINT nFlags, CPoint point)
{
	Hide();
}

void CToolTip::OnLButtonDown(UINT nFlags, CPoint point)
{
	Hide();
}

void CToolTip::OnActivateApp(BOOL bActive, DWORD hTask)
{
	Hide();
}

void CToolTip::OnTimer(UINT_PTR nIDEvent)
{
	Hide();
}

void CToolTip::Hide()
{
	KillTimer(ID_TIMER_POPUP);
	ShowWindow(SW_HIDE);
}


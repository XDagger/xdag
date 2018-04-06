/***************************************************************************
This software will show a squared tooltip.
OVERFLOWS CHECKING WILL NOT BE STRESSED
since the applications are not critical.

Copyright (C) 2018  Marco Scarlino <marco.scarlino@students-live.uniroma2.it>.

Tooltip is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
***************************************************************************/

/********* Overflows checking will not be stressed ********/
/********* since the applications are not critical.  ******/



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
	rgnRect.CreateRectRgn(0,0,0,0);
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

	CRect t_Rect;
	GetClientRect(&t_Rect);

	LONG RectWidth = rect.right;
	LONG RectHeight = rect.bottom;

	rectText.left = 0;
	rectText.right = RectWidth;
	rectText.top = 0;
	rectText.bottom = RectHeight;

	/****!!	 Possible overflow	!!****/
	rgnRect.SetRectRgn(0, 0, (int)RectWidth, (int)RectHeight);
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

    dc.FillRgn(&rgnRect, &brFillBrush);
    dc.FrameRgn(&rgnRect, &brOutlineBrush, 1, 1);

    int nBkMode = dc.SetBkMode(TRANSPARENT);
    COLORREF clrPrevious =  dc.SetTextColor(RGB(0, 0, 0));

    dc.DrawText(strMessage, rectText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, 0);

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
	if (GetSafeHwnd() == NULL) {
		PCSTR pstrOwnerClass = ::AfxRegisterWndClass(0);
		BOOL bResult = CFrameWnd::Create(pstrOwnerClass, NULL, WS_OVERLAPPED, rect);
	}

	int CaptionBarSize = ::GetSystemMetrics(SM_CYCAPTION);
	int VerticalBorderSize = ::GetSystemMetrics(SM_CYSIZEFRAME);
	
	SetWindowPos(
		&wndTopMost,
		rect.left,
		(rect.top -CaptionBarSize -VerticalBorderSize),
		rect.right,		//Width
		rect.bottom,	//Height
		SWP_SHOWWINDOW | SWP_NOACTIVATE
	);
	
    return TRUE;
}


int CToolTip::CalculateRectSizeAndPosition(CPoint pt, int CharWidth, int CharHeight)
{
	int RectLeft, RectWidth, RectTop, RectHeight;

	/****!!	 Possible overflow	!!****/
	int TextLength = strMessage.GetLength() * CharWidth;
	int Height = (int)(CharHeight*1.1);
	RectLeft = (int)pt.x;
	RectWidth = (int)(TextLength * 1.1);
	RectTop = (int)pt.y;
	/*********************************/

	RectHeight = Height;
	rect=CRect(RectLeft, RectTop, RectWidth, RectHeight);
	return 0;
}

//Entry Point
int CToolTip::Show(CPoint pt, LPRECT lpRect,
	int CharWidth, int CharHeight, CString strMessage, UINT Secs)
{
	if (Secs <= 0)
	{
		fprintf(stderr, "ToolTip timer need nSec > 0\n");
		return -1;
	}

	this->strMessage = strMessage;
	CalculateRectSizeAndPosition(pt, CharWidth, CharHeight);
	Create();

	SetTimer(ID_TIMER_POPUP, (Secs * 1000), NULL);

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


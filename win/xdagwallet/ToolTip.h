#pragma once
#include "stdafx.h"

#define ID_TIMER_POPUP 141

class CToolTip : public CFrameWnd
{
    DECLARE_MESSAGE_MAP()

public:
	CToolTip();
	void Hide();
	int Show(CPoint pt, LPRECT lpRect,
		int CharWidth, int nCharHeight, CString strMessage,UINT Secs);

protected:

    ~CToolTip();
    BOOL Create();
	int CalculateRectSizeAndPosition(CPoint pt, int CharWidth, int CharHeight);

	void CToolTip::OnLButtonDown(UINT nFlags, CPoint point);
	void CToolTip::OnRButtonDown(UINT nFlags, CPoint point);

// Overrides messages
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnActivateApp(BOOL bActive, DWORD hTask);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// Attributes
	CRect   rectText; 
    CString strMessage;
	CRgn    rgnRect;   // The region of the rectangle  
	CRect	rect;
    CWnd    m_wndInvisibleParent; // invisible taskbar window to contain the tooltip
};

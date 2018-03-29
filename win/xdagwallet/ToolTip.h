#pragma once
#include "stdafx.h"

#define ID_TIMER_POPUP 141

class CToolTip : public CFrameWnd
{
    DECLARE_MESSAGE_MAP()

public:

    static CToolTip* Show(
    		CPoint pt,				// point where there will be the tooltip 
		LPRECT lpRect,		// rect of the parent
		int nWidth,				// mean character width
		int nHeight,			// mean character height
		CString strMessage,
		UINT nSecs
            );
	    
	static CWnd	* pParentWindow;
	

protected:

    CToolTip(CString strMessage);
    ~CToolTip();
    BOOL Create(CRect rect);
    void MakeVisisble(UINT nSecs);	

// Overrides messages
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnActivateApp(BOOL bActive, DWORD hTask);
    	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// Attributes
	CRect   m_rectText; 
   	CRect   m_rectWindow;   	 // Rectangle of the tooltip
    	CString m_strMessage;
	CRgn    m_rgnRoundRect;   // The region of the round rectangle  
    	CWnd    m_wndInvisibleParent; // invisible taskbare window to contain the tooltip
};

// CEditWalletAddrMine.cpp : implementation file
//

#include "CEditWalletAddrMine.h"

CEditWalletAddrMine::CEditWalletAddrMine()
{
}

BEGIN_MESSAGE_MAP(CEditWalletAddrMine, CEditWalletAddr)
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

// CEditWalletAddrMine message handlers

void CEditWalletAddrMine::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CEditWalletAddr::OnLButtonDblClk(nFlags, point);
	CEdit::Copy();

	CRect rect;
	this->GetWindowRect(&rect);

	CPoint pt = rect.CenterPoint();
	HDC hdc = ::GetDC(this->m_hWnd);

	TEXTMETRIC tm;
	GetTextMetrics(hdc, &tm);

	int nWidth = tm.tmAveCharWidth;
	int nHeight = tm.tmHeight;

	GetClientRect(&rect);
	ToolTip = CToolTip::Show(pt, &rect, nWidth, nHeight,"Copied!", 1);
	
}

// CEditWalletAddrMine.cpp : implementation file
//

#include "CEditWalletAddrMine.h"

CEditWalletAddrMine::CEditWalletAddrMine()
{
}

BEGIN_MESSAGE_MAP(CEditWalletAddrMine, CEditWalletAddr)
	ON_WM_LBUTTONDBLCLK()
	ON_MESSAGE(WM_HIDE_TOOLTIP, &CEditWalletAddrMine::HideTooltip)
END_MESSAGE_MAP()

// CEditWalletAddrMine message handlers
LRESULT CEditWalletAddrMine::HideTooltip(WPARAM wParam, LPARAM lParam)
{
	if (_toolTip != NULL)
		_toolTip->Hide();
	return 0;
}

void CEditWalletAddrMine::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CEditWalletAddr::OnLButtonDblClk(nFlags, point);
	CEdit::Copy();

	if (_toolTip == NULL)
		_toolTip = new CToolTip();

	CRect rect;
	this->GetWindowRect(&rect);

	CPoint pt = rect.CenterPoint();
	HDC hdc = ::GetDC(this->m_hWnd);

	TEXTMETRIC tm;
	GetTextMetrics(hdc, &tm);

	int width = tm.tmAveCharWidth;
	int height = tm.tmHeight;

	GetClientRect(&rect);
	_toolTip->Show(pt, &rect, width, height, "Copied!", 2);
}

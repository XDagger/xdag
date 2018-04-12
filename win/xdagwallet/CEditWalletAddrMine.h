// CEditWalletAddrMine.h : header file
//

#pragma once

#include "stdafx.h"
#include "ToolTip.h"

class CEditWalletAddrMine : public CEditWalletAddr
{

public:
	CEditWalletAddrMine();

protected:

	CToolTip* _toolTip = NULL;
	afx_msg LRESULT HideTooltip(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);

	DECLARE_MESSAGE_MAP()
};



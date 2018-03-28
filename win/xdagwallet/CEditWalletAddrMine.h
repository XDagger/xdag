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

	CToolTip* ToolTip;


	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);



	DECLARE_MESSAGE_MAP()
};



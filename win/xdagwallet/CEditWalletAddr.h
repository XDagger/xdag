// CEditWalletAddr.h : header file
//

#pragma once

#include "afxwin.h"

class CEditWalletAddr : public CEdit
{

public:
	CEditWalletAddr();

protected:

	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);


	DECLARE_MESSAGE_MAP()
};



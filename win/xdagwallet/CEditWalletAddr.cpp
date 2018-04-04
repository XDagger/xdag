// CEditWalletAddr.cpp : implementation file
//

#include "CEditWalletAddr.h"



CEditWalletAddr::CEditWalletAddr()
{

}


BEGIN_MESSAGE_MAP(CEditWalletAddr, CEdit)
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()





// CEditWalletAddr message handlers


void CEditWalletAddr::OnLButtonDblClk(UINT nFlags, CPoint point)
{

	CEdit::SetSel(0,-1);

}

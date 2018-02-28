
// cheatcoinwalletDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "PasswordDlg.h"
#include "../../client/main.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Password dialog

CPasswordDlg::CPasswordDlg() : CDialogEx(IDD_DIALOG_PASSWORD)
{
}

void CPasswordDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_PASSWORD, password);
}

BEGIN_MESSAGE_MAP(CPasswordDlg, CDialogEx)
	ON_BN_CLICKED(IDOK, &CPasswordDlg::OnBnClickedOk)
	ON_WM_ACTIVATE()
END_MESSAGE_MAP()

void CPasswordDlg::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
    CDialogEx::OnActivate(nState, pWndOther, bMinimized);
    SetDlgItemTextW(IDC_STATIC1, passwd);
    UpdateData(false);
}

void CPasswordDlg::OnBnClickedOk()
{
	UpdateData(true);
	len = password.GetLine(0, passwd, 256);
	// TODO: Add your control notification handler code here
	CDialogEx::OnOK();
}

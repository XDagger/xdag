
// cheatcoinwalletDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "PasswordDlg.h"
#include "../../client/init.h"

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
	DDX_Text(pDX, IDC_EDIT_PASSWORD, _password);
	DDX_Text(pDX, IDC_STATIC_PASSWORD_PROMT, _passwordPromt);
}

BEGIN_MESSAGE_MAP(CPasswordDlg, CDialogEx)
	ON_BN_CLICKED(IDOK, &CPasswordDlg::OnBnClickedOk)
	ON_WM_ACTIVATE()
END_MESSAGE_MAP()

BOOL CPasswordDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	UpdateData(false);
	return TRUE;
}

void CPasswordDlg::OnBnClickedOk()
{
	UpdateData(true);
	CDialogEx::OnOK();
}

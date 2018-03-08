// cheatcoinwalletDlg.h : header file
//

#pragma once

#include "afxwin.h"
#include "afxdialogex.h"

class CPasswordDlg : public CDialogEx
{
public:
    CPasswordDlg();

    // Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG1 };
#endif

protected:
	virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    DECLARE_MESSAGE_MAP()
private:
	CString _passwordPromt;
	CString _password;
    afx_msg void OnBnClickedOk();

public:
	void SetPasswordPromt(CString passwordPromt) { _passwordPromt = passwordPromt; }
	CString GetPassword() { return _password; }
};

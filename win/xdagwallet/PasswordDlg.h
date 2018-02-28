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
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
    DECLARE_MESSAGE_MAP()
public:
    CEdit password;
    afx_msg void OnBnClickedOk();
    wchar_t passwd[256];
    int len;
    afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
};

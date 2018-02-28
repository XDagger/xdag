// cheatcoinwalletDlg.h : header file
//

#pragma once

#include "afxwin.h"

// CXDagWalletDlg dialog
class CXDagWalletDlg : public CDialog
{
// Construction
public:
	CXDagWalletDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_XDAGWALLET_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

private:
    CButton _applyButton;
    CString _poolAddress;
    int _miningThreadsCount;
    CString _hashRate;
    CString _balance;
    CString _accountAddress;
    CString _transferAmount;
    CString _transferAddress;

    afx_msg void OnClickedButtonConnect();
    afx_msg void OnClickedButtonXfer();
    afx_msg void OnBnClickedButtonApply();

	static int InputPassword(const char *prompt, char *buf, unsigned size);
	static int ShowState(const char *state, const char *balance, const char *address);
	
public:
	void SetBalance(CString balance) { _balance = balance; }
	void SetAccountAddress(CString accountAddress) { _accountAddress = accountAddress; }
};

extern CXDagWalletDlg *g_dlg;

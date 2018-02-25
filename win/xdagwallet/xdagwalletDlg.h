
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
public:
	afx_msg void OnClickedButtonConnect();
	CEdit pooladdr;
	CEdit nthreads;
private:
	static int inputPassword(const char *prompt, char *buf, unsigned size);
	static int showState(const char *state, const char *balance, const char *address);

public:
	CEdit balance;
	CEdit address;
	CEdit amount;
	CEdit transfer;
	afx_msg void OnClickedButtonXfer();
};

extern CXDagWalletDlg *g_dlg;

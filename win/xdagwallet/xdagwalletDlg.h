// cheatcoinwalletDlg.h : header file
//

#pragma once

#include "afxwin.h"
#include "CEditWalletAddrMine.h"

#define WM_UPDATE_STATE (WM_USER+1)
#define ID_TIMER_INIT 100

// CXDagWalletDlg dialog
class CXDagWalletDlg : public CDialog {
	// Construction
public:
	CXDagWalletDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_XDAGWALLET_DIALOG };
#endif

protected:
	HICON m_hIcon;

	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);	

	// Generated message map functions
	BOOL OnInitDialog() override;
	void OnOK() override;
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

private:
	CString _balance;
	CEdit _balanceEdit;
	CString _accountAddress;
	CEditWalletAddrMine _accountAddressEdit;
	CString _transferAmount;
	CString _transferAddress;
	CString _state;
	CStatic _stateControl;
	CEdit _transferAmountEdit;
	CEditWalletAddr _transferAddressEdit;
	CButton _xferButton;
	bool _firstActivate;

	afx_msg void OnClickedButtonXfer();
	afx_msg LRESULT OnUpdateState(WPARAM wParam, LPARAM lParam);
	afx_msg void OnTimer(WPARAM wParam);

	static int InputPassword(const char *prompt, char *buf, unsigned size);
	static int ShowState(const char *state, const char *balance, const char *address);

public:
	void SetBalance(const char *balance) { _balance = balance; }
	void SetAccountAddress(const char *address) { _accountAddress = address; }
	void SetState(const char *state) { _state = state; }
};

extern CXDagWalletDlg *g_dlg;




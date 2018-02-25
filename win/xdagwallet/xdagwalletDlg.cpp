
// cheatcoinwalletDlg.cpp : implementation file
//

#include "stdafx.h"
#include "xdagwallet.h"
#include "xdagwalletDlg.h"
#include "afxdialogex.h"
#include "../../client/main.h"
#include "afxwin.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CXDagWalletDlg *g_dlg = 0;

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// Password dialog used for App About

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

														// Implementation
protected:
	DECLARE_MESSAGE_MAP()
public:
	CEdit password;
	afx_msg void OnBnClickedOk();
	wchar_t passwd[256];
	int len;
	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
};

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

// CXDagWalletDlg dialog

CXDagWalletDlg::CXDagWalletDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_XDAGWALLET_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CXDagWalletDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_PASSWORD, pooladdr);
	DDX_Control(pDX, IDC_EDIT_MINING_THREADS, nthreads);
	DDX_Control(pDX, IDC_EDIT_BALANCE, balance);
	DDX_Control(pDX, IDC_EDIT_ACCOUNT_ADDRESS, address);
	DDX_Control(pDX, IDC_EDIT_TRANSFER_AMOUNT, amount);
	DDX_Control(pDX, IDC_EDIT_TRANSFER_TO, transfer);
}

BEGIN_MESSAGE_MAP(CXDagWalletDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CXDagWalletDlg::OnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_XFER, &CXDagWalletDlg::OnClickedButtonXfer)
END_MESSAGE_MAP()


// CXDagWalletDlg message handlers

BOOL CXDagWalletDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CXDagWalletDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CXDagWalletDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CXDagWalletDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

int CXDagWalletDlg::inputPassword(const char *prompt, char *buf, unsigned size) {
	int i, len;
	CPasswordDlg dlgPwd;
	for (i = 0; i < 254 && prompt[i]; ++i) dlgPwd.passwd[i] = prompt[i];
	dlgPwd.passwd[i] = ':';
	dlgPwd.passwd[i+1] = 0;
	dlgPwd.DoModal();
	len = dlgPwd.len;
	if (len < 0) len = 0;
	if (len >= size) len = size - 1;
	for (i = 0; i < len; ++i) buf[i] = dlgPwd.passwd[i];
	buf[len] = 0;
	return 0;
}

int CXDagWalletDlg::showState(const char *state, const char *balance, const char *address) {
	wchar_t wbalance[64], waddress[64], wstate[256];
	int i;
	for (i = 0; i < 64; ++i) wbalance[i] = balance[i];
	for (i = 0; i < 64; ++i) waddress[i] = address[i];
	for (i = 0; i < 256; ++i) wstate[i] = state[i];
	g_dlg->balance.SetWindowTextW(wbalance);
	g_dlg->address.SetWindowTextW(waddress);
	g_dlg->SetDlgItemTextW(IDC_STATIC8, wstate);
	g_dlg->UpdateData(false);
	return 0;
}

void CXDagWalletDlg::OnClickedButtonConnect()
{
	wchar_t wpoolbuf[256], wnthreadsbuf[16];
	char *poolbuf = new char[256], *nthreadsbuf = new char[16];
	int argc = 4, i, poollen, thrlen;
	UpdateData(true);
	poollen = pooladdr.GetLine(0, wpoolbuf, 256);
	thrlen = nthreads.GetLine(0, wnthreadsbuf, 16);
	if (poollen < 0) poollen = 0;
	if (thrlen < 0) thrlen = 0;
	for (i = 0; i < poollen; ++i) poolbuf[i] = wpoolbuf[i];
	for (i = 0; i < thrlen; ++i) nthreadsbuf[i] = wnthreadsbuf[i];
	poolbuf[poollen] = 0;
	nthreadsbuf[thrlen] = 0;
	char *argv[] = { "xdag.exe", "-m", nthreadsbuf, poolbuf };
	xdag_set_password_callback(&inputPassword);
	g_xdag_show_state = &showState;
	xdag_main(argc, argv);
	// TODO: Add your control notification handler code here
}


void CPasswordDlg::OnBnClickedOk()
{
	UpdateData(true);
	len = password.GetLine(0, passwd, 256);
	// TODO: Add your control notification handler code here
	CDialogEx::OnOK();
}


void CPasswordDlg::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	CDialogEx::OnActivate(nState, pWndOther, bMinimized);
	SetDlgItemTextW(IDC_STATIC1, passwd);
	UpdateData(false);
	// TODO: Add your message handler code here
}


void CXDagWalletDlg::OnClickedButtonXfer()
{
	// TODO: Add your control notification handler code here
	wchar_t wamountbuf[64], wxferbuf[64];
	char amountbuf[64], xferbuf[64];
	int i, amountlen, xferlen;
	UpdateData(true);
	amountlen = amount.GetLine(0, wamountbuf, 64);
	xferlen = transfer.GetLine(0, wxferbuf, 64);
	if (amountlen < 0) amountlen = 0;
	if (xferlen < 0) xferlen = 0;
	for (i = 0; i < amountlen; ++i) amountbuf[i] = wamountbuf[i];
	for (i = 0; i < xferlen; ++i) xferbuf[i] = wxferbuf[i];
	amountbuf[amountlen] = 0;
	xferbuf[xferlen] = 0;
	xdag_do_xfer(0, amountbuf, xferbuf);
}

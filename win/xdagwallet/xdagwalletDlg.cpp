
// cheatcoinwalletDlg.cpp : implementation file
//

#include "stdafx.h"
#include "xdagwallet.h"
#include "xdagwalletDlg.h"
#include "afxdialogex.h"
#include "../../client/main.h"
#include "PasswordDlg.h"

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

// CXDagWalletDlg dialog

CXDagWalletDlg::CXDagWalletDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_XDAGWALLET_DIALOG, pParent)
    , _poolAddress(_T(""))
    , _miningThreadsCount(0)
    , _hashRate(_T(""))
    , _balance(_T(""))
    , _accountAddress(_T(""))
    , _transferAmount(_T(""))
    , _transferAddress(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CXDagWalletDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_BUTTON_APPLY, _applyButton);
    DDX_Text(pDX, IDC_EDIT_POOL_ADDRESS, _poolAddress);
    DDV_MaxChars(pDX, _poolAddress, 64);
    DDX_Text(pDX, IDC_EDIT_MINING_THREADS, _miningThreadsCount);
    DDV_MinMaxInt(pDX, _miningThreadsCount, 0, 999);
    DDX_Text(pDX, IDC_EDIT_HASHRATE, _hashRate);
    DDX_Text(pDX, IDC_EDIT_BALANCE, _balance);
    DDX_Text(pDX, IDC_EDIT_ACCOUNT_ADDRESS, _accountAddress);
    DDX_Text(pDX, IDC_EDIT_TRANSFER_AMOUNT, _transferAmount);
    DDV_MaxChars(pDX, _transferAmount, 32);
    DDX_Text(pDX, IDC_EDIT_TRANSFER_TO, _transferAddress);
	DDV_MaxChars(pDX, _transferAddress, 64);
}

BEGIN_MESSAGE_MAP(CXDagWalletDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CXDagWalletDlg::OnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_XFER, &CXDagWalletDlg::OnClickedButtonXfer)
    ON_BN_CLICKED(IDC_BUTTON_APPLY, &CXDagWalletDlg::OnBnClickedButtonApply)
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
		CString strAboutMenu;
		BOOL bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
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

	_applyButton.EnableWindow(FALSE);

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

int CXDagWalletDlg::InputPassword(const char *prompt, char *buf, unsigned size) 
{
	CPasswordDlg dlgPwd;
	dlgPwd.SetPasswordPromt(CString(prompt));
	dlgPwd.DoModal();
	strncpy(buf, (char*)(LPCTSTR)dlgPwd.GetPassword(), size);
	return 0;
}

int CXDagWalletDlg::ShowState(const char *state, const char *balance, const char *address) 
{
	/*wchar_t wbalance[64], waddress[64], wstate[256];
	int i;
	for (i = 0; i < 64; ++i) wbalance[i] = balance[i];
	for (i = 0; i < 64; ++i) waddress[i] = address[i];
	for (i = 0; i < 256; ++i) wstate[i] = state[i];
	g_dlg->balance.SetWindowTextW(wbalance);
	g_dlg->address.SetWindowTextW(waddress);*/
	g_dlg->SetBalance(CString(balance));
	g_dlg->SetAccountAddress(CString(address));
	g_dlg->SetDlgItemTextW(IDC_STATIC_STATE, CString(state));
	g_dlg->UpdateData(false);
	return 0;
}

void CXDagWalletDlg::OnClickedButtonConnect()
{
	UpdateData(true);
	char buf[10];
	char *argv[] = { "xdag.exe", "-m", _itoa(_miningThreadsCount, buf, 10), (char*)(LPCTSTR)_poolAddress };
	xdag_set_password_callback(&InputPassword);
	g_xdag_show_state = &ShowState;
	xdag_main(4, argv);
}

void CXDagWalletDlg::OnClickedButtonXfer()
{
	UpdateData(true);
	xdag_do_xfer(0, (char*)(LPCTSTR)_transferAmount, (char*)(LPCTSTR)_transferAddress);
}

void CXDagWalletDlg::OnBnClickedButtonApply()
{
    
}

#include "ToolTip.h"

BEGIN_MESSAGE_MAP(CToolTip, CFrameWnd)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_TIMER()
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_ACTIVATEAPP()
END_MESSAGE_MAP()

// Constructor
CToolTip::CToolTip(CString strMessage)
{
    m_strMessage       = strMessage;
}

//Destructor
CToolTip::~CToolTip()
{
}

//Contructor
CToolTip* CToolTip::Show(CPoint pt,LPRECT lpRect, 
				int nCharWidth, int nCharHeight, CString strMessage, UINT nSecs)
{
	CToolTip* pToolTip = new CToolTip(strMessage);

	int nRectLeft;
	int nRectRight;
	int nRectTop;  
	int nRectBottom;

	int nTextLength = strMessage.GetLength() * nCharWidth;
	int nHeight = 0;

#pragma warning( push )
#pragma warning( disable : 4244)
	if (conversion_toInt(((nCharHeight) *3.5), &nHeight)) 
	{
		fprintf(stderr, "overflow error\n");
		exit(EXIT_FAILURE);
	}
#pragma warning( pop ) 

	int int_nTextLength = 0;
#pragma warning( push )
#pragma warning( disable : 4244)
	if (conversion_toInt((nTextLength * 0.6), &int_nTextLength)) 
	{
		fprintf(stderr, "overflow error\n");
		exit(EXIT_FAILURE);
	}
#pragma warning( pop ) 

	nRectLeft = pt.x - int_nTextLength;
	nRectRight  = pt.x + int_nTextLength;

	nRectTop    = pt.y - nHeight;
	nRectBottom = pt.y;
    
	pToolTip->Create(CRect(nRectLeft, nRectTop, nRectRight, nRectBottom));    
    	pToolTip->MakeVisisble(nSecs);

    	return pToolTip;
}


void CToolTip::MakeVisisble(UINT nSecs)
{
	if (nSecs<=0)
	{
		fprintf(stderr, "ToolTip timer need nSec > 0\n");
		exit(EXIT_FAILURE);
	}

    	SetTimer(ID_TIMER_POPUP, (nSecs * 1000), NULL);
        
    	CRect rect;
    	GetWindowRect(&rect);

    	int nCaptionBarSize = ::GetSystemMetrics(SM_CYCAPTION);
    	int nVerticalBorderSize = ::GetSystemMetrics(SM_CYSIZEFRAME);

 

    	SetWindowPos(
		&wndTopMost,
		m_rectWindow.left, 
		(m_rectWindow.top + nCaptionBarSize + (2 * nVerticalBorderSize)),
		m_rectWindow.right,
		m_rectWindow.bottom, 
		SWP_SHOWWINDOW  | SWP_NOACTIVATE
        );
    

}	

int CToolTip::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    	{
        	return -1;
    	}	
	
   	ModifyStyle(WS_CAPTION , 0); 

    	CRect t_Rect;
	GetClientRect(&t_Rect); 

 

	long long_RectWidth = 0;
	long long_RectHeight = 0;


#pragma warning( push )
#pragma warning( disable : 4244)
	if (conversion_toLong((t_Rect.Width() * 0.90), &long_RectWidth)) 
	{
		fprintf(stderr, "overflow error\n");
		exit(EXIT_FAILURE);
	}
	if (conversion_toLong((t_Rect.Height()* 0.90), &long_RectHeight)) 
	{
		fprintf(stderr, "overflow error\n");
		exit(EXIT_FAILURE);
	}
    	m_rectText.left   = t_Rect.Width() * 0.10; 
    	m_rectText.right  = long_RectWidth;
    	m_rectText.top    = t_Rect.Height() * 0.10; 
   	m_rectText.bottom = long_RectHeight;
#pragma warning( pop ) 
            	
	m_rgnRoundRect.CreateRectRgn(t_Rect.left+30, t_Rect.top, t_Rect.right-30, t_Rect.bottom);

	CRgn rgnComb;
	rgnComb.CreateRectRgn(t_Rect.left+30, t_Rect.top, t_Rect.right-30, t_Rect.bottom);
	SetWindowRgn(rgnComb.operator HRGN(), TRUE);

	return 0;
}

void CToolTip::OnPaint() 
{
	CPaintDC dc(this); 

    	CRect t_Rect;
    	GetClientRect(&t_Rect);

	CBrush brOutlineBrush;
    	brOutlineBrush.CreateSolidBrush(RGB(0, 0, 0)); 
   
    	CBrush brFillBrush;
	COLORREF crBackground = ::GetSysColor(COLOR_INFOBK);
    	brFillBrush.CreateSolidBrush(crBackground);

    	dc.FillRgn(&m_rgnRoundRect, &brFillBrush);
    	dc.FrameRgn(&m_rgnRoundRect, &brOutlineBrush, 1, 1);

    	int nBkMode = dc.SetBkMode(TRANSPARENT);
    	COLORREF clrPrevious =  dc.SetTextColor(RGB(0, 0, 0));

    	dc.DrawText(m_strMessage, m_rectText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    	dc.SetBkColor(nBkMode);
    	dc.SetTextColor(clrPrevious);
}

BOOL CToolTip::PreCreateWindow(CREATESTRUCT& cs)
{
	// Override the creation of the new default window with an invisible one
	// and join this one with the already existant.
	
	if (!CWnd::PreCreateWindow(cs))
    	{
		return FALSE;
    	}

 	if (!::IsWindow(m_wndInvisibleParent.m_hWnd))
 	{
       		PCSTR pstrOwnerClass = ::AfxRegisterWndClass(0);
       
        	BOOL bError = m_wndInvisibleParent.CreateEx(0,pstrOwnerClass,  _T(""), WS_POPUP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, 0);

        	if (bError == FALSE)
        	{
			return FALSE;
        	}
		
 	}

	cs.hwndParent = m_wndInvisibleParent.m_hWnd;
	return TRUE;
}

BOOL CToolTip::Create(CRect rect)
{
    m_rectWindow = rect;

    PCSTR pstrOwnerClass = ::AfxRegisterWndClass(0);
    BOOL bResult = CFrameWnd::Create(pstrOwnerClass, NULL, WS_OVERLAPPED, m_rectWindow);
    
    return bResult;
}

void CToolTip::OnActivateApp(BOOL bActive, DWORD hTask) 
{
	if(KillTimer(ID_TIMER_POPUP))
	{
		try
		{
			DestroyWindow();
		}
		catch (...)
		{

		}
	}
}

void CToolTip::OnTimer(UINT_PTR nIDEvent)
{
	if (KillTimer(ID_TIMER_POPUP))
	{
		try
		{
			DestroyWindow();
		}
		catch (...)
		{

		}
	}
}

#include "pch.h"
#include "CSetupDlg.h"

CSetupDlg::CSetupDlg(CWnd* pParent)
	: CDialogEx(IDD_SETUP_DIALOG, pParent)
	, m_pScrollView(nullptr)
{
}

CSetupDlg::~CSetupDlg()
{
	if (m_pScrollView != nullptr)
	{
		delete m_pScrollView;
		m_pScrollView = nullptr;
	}
}

BOOL CSetupDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	CWnd* pWndPos = GetDlgItem(IDC_STATIC_SETUP_LIST);
	if (pWndPos == nullptr)
	{
		return TRUE;
	}

	CRect rect;
	pWndPos->GetWindowRect(&rect);
	ScreenToClient(&rect);
	pWndPos->ShowWindow(SW_HIDE);

	m_pScrollView = new CListScrollView(ITEM_SETUP);
	if (m_pScrollView->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER,
		rect, this, 50001))
	{
		m_pScrollView->OnInitialUpdate();
		m_pScrollView->AddItem();
		m_pScrollView->AddItem();
		m_pScrollView->AddItem();
	}

	return TRUE;
}

void CSetupDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSetupDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_SYSTEM_SAVE, &CSetupDlg::OnBnClickedBtSystemSave)
	ON_BN_CLICKED(IDC_BTN_SYSTEM_EXIT, &CSetupDlg::OnBnClickedBtSystemExit)
END_MESSAGE_MAP()

void CSetupDlg::OnBnClickedBtSystemSave()
{
}

void CSetupDlg::OnBnClickedBtSystemExit()
{
	EndDialog(IDCANCEL);
}



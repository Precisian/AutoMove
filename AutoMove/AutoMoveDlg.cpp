// AutoMoveDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "AutoMove.h"
#include "AutoMoveDlg.h"
#include "afxdialogex.h"
#include "CPathItem.h"
#include "CDriveManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	constexpr UINT_PTR TIMER_PATHITEM_BLINK = 1;
	constexpr UINT TIMER_PATHITEM_BLINK_INTERVAL = 1000;
	constexpr DWORD DRIVE_USAGE_CHECK_INTERVAL = 60000;
}


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
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


// CAutoMoveDlg 대화 상자

CAutoMoveDlg::CAutoMoveDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_AUTOMOVE_DIALOG, pParent)
	, m_pScrollView(nullptr)
	, m_bPathItemBlinkOn(TRUE)
	, m_bPathItemBlinkTimerActive(FALSE)
	, m_pDriveUsageThread(nullptr)
	, m_hDriveUsageStopEvent(nullptr)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CAutoMoveDlg::~CAutoMoveDlg()
{
	StopDriveUsageThread();
}

void CAutoMoveDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAutoMoveDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_MAIN_EXIT, &CAutoMoveDlg::OnBnClickedMainExit)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BTN_SETUP_OPEN, &CAutoMoveDlg::OnBnClickedBtSystemOpen)
	ON_MESSAGE(WM_TRAY_ICON, &CAutoMoveDlg::OnTrayIcon)
	ON_COMMAND(ID_TRAY_OPEN, &CAutoMoveDlg::OnTrayOpen)
	ON_COMMAND(ID_TRAY_EXIT, &CAutoMoveDlg::OnTrayExit)
	ON_MESSAGE(WM_PATHITEM_STATE_CHANGED, &CAutoMoveDlg::OnPathItemStateChanged)
END_MESSAGE_MAP()


// CAutoMoveDlg 메시지 처리기

BOOL CAutoMoveDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
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

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	EnableDynamicLayout(TRUE);
	AlignControls();
	SetTrayIcon();
	m_pParam.Load();
	LoadAvailableDriveNames();
	StartDriveUsageThread();

	// 1. Picture Control(IDC_STATIC_LIST_ITEM) 영역 좌표 가져오기
	CRect rect;
	CWnd* pWndPos = GetDlgItem(IDC_STATIC_MAIN_LIST);
	pWndPos->GetWindowRect(&rect);
	ScreenToClient(&rect);
	pWndPos->ShowWindow(SW_HIDE); // 가이드용 컨트롤은 숨김

	// 2. 스크롤뷰 동적 생성
	m_pScrollView = new CListScrollView(ITEM_PATH);

	// 3. WS_VSCROLL 스타일을 추가하여 내장 수직 스크롤바 활성화
	if (!m_pScrollView->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER,
		rect, this, 50001)) {
		return FALSE;
	}

	m_pScrollView->OnInitialUpdate();
	ReloadPathItems();
	UpdatePathItemBlinkTimer();

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CAutoMoveDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else if ((nID & 0xFFF0) == SC_CLOSE)
	{
		// 우측 상단 닫기 버튼을 누르면 최소화
		ShowWindow(SW_MINIMIZE);
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CAutoMoveDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CAutoMoveDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CAutoMoveDlg::OnBnClickedMainExit()
{
	int ret = MessageBox(_T("프로그램을 종료하시겠습니까?"), _T("종료 확인"), MB_YESNO | MB_ICONQUESTION);
	if (ret == IDYES)
	{
		KillTimer(TIMER_PATHITEM_BLINK);
		StopDriveUsageThread();
		EndDialog(IDOK);
	}
}


void CAutoMoveDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	// 예: 창 크기를 800x600으로 완전히 고정하고 싶을 때
	lpMMI->ptMinTrackSize.x = 400; // 최소 가로
	lpMMI->ptMinTrackSize.y = 300; // 최소 세로
	lpMMI->ptMaxTrackSize.x = 400; // 최대 가로
	lpMMI->ptMaxTrackSize.y = 300; // 최대 세로

	CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CAutoMoveDlg::AlignControls()
{
	CRect rectClient;
	GetClientRect(&rectClient);
	int cx = rectClient.Width();

	// 1. 컨트롤 가져오기
	CWnd* pBtnClose = GetDlgItem(IDC_BTN_MAIN_EXIT);    // 가장 우측 버튼 (1번)
	CWnd* pBtnSetup = GetDlgItem(IDC_BTN_SETUP_OPEN); // 그 옆의 버튼 (2번)
	CWnd* pPic = GetDlgItem(IDC_STATIC_MAIN_LIST);

	int firstBtnX = 0; // 두 번째 버튼의 기준점이 될 좌표
	const int RIGHT_MARGIN = 10; // 우측 끝단 마진
	const int BOTTOM_MARGIN = 10; // 하단 마진
	const int ELEMENT_GAP = 10;  // 버튼 사이의 간격

	// 2. 가장 우측 버튼(Close) 정렬
	
	if (pBtnClose && pBtnClose->GetSafeHwnd()) {
		CRect r;
		pBtnClose->GetWindowRect(&r);
		ScreenToClient(&r);

		// 우측 끝에서 마진만큼 띄움
		firstBtnX = cx - r.Width() - RIGHT_MARGIN;
		pBtnClose->MoveWindow(firstBtnX, r.top, r.Width(), r.Height());
	}

	// 3. 두 번째 버튼(System) 정렬 - 동일 마진 적용
	if (pBtnSetup && pBtnSetup->GetSafeHwnd()) {
		CRect r;
		pBtnSetup->GetWindowRect(&r);
		ScreenToClient(&r);

		// 첫 번째 버튼의 시작점(firstBtnX)에서 간격과 자신의 너비를 뺌
		firstBtnX = cx - r.Width() - RIGHT_MARGIN;
		pBtnSetup->MoveWindow(firstBtnX, r.top, r.Width(), r.Height());
	}

	// 4. Picture Control 및 스크롤뷰 영역 업데이트
	if (pPic && pPic->GetSafeHwnd()) {
		CRect rPic;
		pPic->GetWindowRect(&rPic);
		ScreenToClient(&rPic);

		// 우측 끝 마진에 맞게 너비 조절
		int newPicWidth = cx - rPic.left - RIGHT_MARGIN;

		// 하단 끝 마진에 맞게 높이 조절
		int newPicHeight = rectClient.Height() - rPic.top - BOTTOM_MARGIN;
		pPic->MoveWindow(rPic.left, rPic.top, newPicWidth, newPicHeight);

		if (m_pScrollView && m_pScrollView->GetSafeHwnd()) {
			m_pScrollView->MoveWindow(rPic.left, rPic.top, newPicWidth, newPicHeight);
		}
	}
}

void CAutoMoveDlg::OnBnClickedBtSystemOpen()
{
	CSetupDlg dlg(this, &m_pParam, m_vecAvailableDriveNames);
	if (dlg.DoModal() == IDOK)
	{
		ReloadPathItems();
	}
}

void CAutoMoveDlg::OnCancel()
{
	// 임의로 종료 방지
}

void CAutoMoveDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_PATHITEM_BLINK)
	{
		m_bPathItemBlinkOn = !m_bPathItemBlinkOn;
		NotifyPathItemBlink();
		return;
	}

	CDialogEx::OnTimer(nIDEvent);
}

LRESULT CAutoMoveDlg::OnPathItemStateChanged(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);

	UpdatePathItemBlinkTimer();
	return 0;
}

void CAutoMoveDlg::SetTrayIcon()
{
	// 트레이 아이콘 설정
	m_nId.cbSize = sizeof(NOTIFYICONDATA);
	m_nId.hWnd = m_hWnd;
	m_nId.uID = 1; 
	m_nId.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	m_nId.uCallbackMessage = WM_TRAY_ICON; 
	m_nId.hIcon = m_hIcon; // 아이콘 핸들
	_tcscpy_s(m_nId.szTip, _T("AutoMove")); 
	Shell_NotifyIcon(NIM_ADD, &m_nId);
}

void CAutoMoveDlg::ReloadPathItems()
{
	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd())
	{
		return;
	}

	m_pScrollView->ClearItems();

	for (int i = 0; i < static_cast<int>(m_pParam.m_vecTemplate.size()); ++i)
	{
		CPathItem* pItem = dynamic_cast<CPathItem*>(m_pScrollView->AddItem());
		if (pItem != nullptr)
		{
			const CParameter::PARAM_TEMPLATE& paramTemplate = m_pParam.m_vecTemplate[i];
			pItem->LoadFromTemplate(paramTemplate);
			pItem->SetBlinkOn(m_bPathItemBlinkOn);
		}
	}

	UpdatePathItemBlinkTimer();
}

void CAutoMoveDlg::NotifyPathItemBlink()
{
	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd())
	{
		return;
	}

	for (int i = 0; i < m_pScrollView->GetItemCount(); ++i)
	{
		CPathItem* pItem = dynamic_cast<CPathItem*>(m_pScrollView->GetItem(i));
		if (pItem != nullptr)
		{
			pItem->SetBlinkOn(m_bPathItemBlinkOn);
		}
	}
}

void CAutoMoveDlg::UpdatePathItemBlinkTimer()
{
	const BOOL bNeedTimer = HasBlinkingPathItem();
	if (bNeedTimer && !m_bPathItemBlinkTimerActive)
	{
		SetTimer(TIMER_PATHITEM_BLINK, TIMER_PATHITEM_BLINK_INTERVAL, nullptr);
		m_bPathItemBlinkTimerActive = TRUE;
		return;
	}

	if (!bNeedTimer && m_bPathItemBlinkTimerActive)
	{
		KillTimer(TIMER_PATHITEM_BLINK);
		m_bPathItemBlinkTimerActive = FALSE;
		m_bPathItemBlinkOn = TRUE;
		NotifyPathItemBlink();
	}
}

BOOL CAutoMoveDlg::HasBlinkingPathItem() const
{
	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd())
	{
		return FALSE;
	}

	for (int i = 0; i < m_pScrollView->GetItemCount(); ++i)
	{
		CPathItem* pItem = dynamic_cast<CPathItem*>(m_pScrollView->GetItem(i));
		if (pItem != nullptr && (pItem->IsWaitingEvent() || pItem->IsWorkingMoveCopy()))
		{
			return TRUE;
		}
	}

	return FALSE;
}

void CAutoMoveDlg::LoadAvailableDriveNames()
{
	CDriveManager driveManager;
	driveManager.LoadAvailableDrives();
	m_vecAvailableDriveNames = driveManager.GetDriveNames();
}

void CAutoMoveDlg::StartDriveUsageThread()
{
	if (m_pDriveUsageThread != nullptr || m_vecAvailableDriveNames.empty())
	{
		return;
	}

	m_hDriveUsageStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (m_hDriveUsageStopEvent == nullptr)
	{
		return;
	}

	m_pDriveUsageThread = AfxBeginThread(DriveUsageThreadProc, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (m_pDriveUsageThread == nullptr)
	{
		CloseHandle(m_hDriveUsageStopEvent);
		m_hDriveUsageStopEvent = nullptr;
		return;
	}

	m_pDriveUsageThread->m_bAutoDelete = FALSE;
	m_pDriveUsageThread->ResumeThread();
}

void CAutoMoveDlg::StopDriveUsageThread()
{
	if (m_hDriveUsageStopEvent != nullptr)
	{
		SetEvent(m_hDriveUsageStopEvent);
	}

	if (m_pDriveUsageThread != nullptr)
	{
		WaitForSingleObject(m_pDriveUsageThread->m_hThread, INFINITE);
		delete m_pDriveUsageThread;
		m_pDriveUsageThread = nullptr;
	}

	if (m_hDriveUsageStopEvent != nullptr)
	{
		CloseHandle(m_hDriveUsageStopEvent);
		m_hDriveUsageStopEvent = nullptr;
	}
}

UINT CAutoMoveDlg::DriveUsageThreadProc(LPVOID pParam)
{
	CAutoMoveDlg* pView = reinterpret_cast<CAutoMoveDlg*>(pParam);
	if (pView == nullptr)
	{
		return 0;
	}

	CDriveManager driveManager(pView->m_vecAvailableDriveNames);

	while (true)
	{
		if (WaitForSingleObject(pView->m_hDriveUsageStopEvent, 0) == WAIT_OBJECT_0)
		{
			return 0;
		}

		driveManager.CheckDriveUsage();

		if (WaitForSingleObject(pView->m_hDriveUsageStopEvent, DRIVE_USAGE_CHECK_INTERVAL) == WAIT_OBJECT_0)
		{
			break;
		}
	}

	return 0;
}

// 트레이 아이콘 메시지 처리
LRESULT CAutoMoveDlg::OnTrayIcon(WPARAM wParam, LPARAM lParam) {
	if (lParam == WM_RBUTTONUP) { 
		CMenu menu, * pSubMenu;
		menu.LoadMenu(IDR_MENU_TRAY);
		pSubMenu = menu.GetSubMenu(0);

		CPoint pt;
		GetCursorPos(&pt);

		// 트레이 메뉴 동작을 위한 필수 설정
		SetForegroundWindow();
		pSubMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
	}
	else if (lParam == WM_LBUTTONDBLCLK) { // 더블 클릭 시 화면 열기
		SendMessage(WM_COMMAND, ID_TRAY_OPEN);
	}
	return 0;
}

void CAutoMoveDlg::OnTrayOpen()
{
	if (IsIconic()) ShowWindow(SW_RESTORE);
	else			ShowWindow(SW_SHOW);
	SetForegroundWindow();
}

void CAutoMoveDlg::OnTrayExit()
{
	Shell_NotifyIcon(NIM_DELETE, &m_nId); // 중요: 종료 시 아이콘 제거
	OnBnClickedMainExit();
}

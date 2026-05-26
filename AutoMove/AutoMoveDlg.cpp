// AutoMoveDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "AutoMove.h"
#include "AutoMoveDlg.h"
#include "afxdialogex.h"
#include "CPathItem.h"
#include "Manager/CDriveManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	constexpr UINT_PTR TIMER_PATHITEM_BLINK = 1;
	constexpr UINT TIMER_PATHITEM_BLINK_INTERVAL = 1000;
	constexpr DWORD DRIVE_USAGE_CHECK_INTERVAL = 60000;
	constexpr int DRIVE_USAGE_GROUP_TOP_PADDING = 18;
	constexpr int DRIVE_USAGE_GROUP_BOTTOM_PADDING = 8;
	constexpr int DRIVE_USAGE_GROUP_INSET_X = 10;
	constexpr int DRIVE_USAGE_ITEM_HEIGHT = 18;
	constexpr int DRIVE_USAGE_ITEM_GAP = 5;
	constexpr int DRIVE_USAGE_BOTTOM_GAP = 8;
	constexpr int DRIVE_USAGE_ITEM_ID_BASE = 60000;
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
	m_driveTaskWorker.Stop();
	StopDriveUsageThread();
	DestroyDriveUsageControls();
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
	ON_MESSAGE(WM_DRIVE_USAGE_UPDATED, &CAutoMoveDlg::OnDriveUsageUpdated)
	ON_MESSAGE(WM_DRIVE_TASK_STARTED, &CAutoMoveDlg::OnDriveTaskStarted)
	ON_MESSAGE(WM_DRIVE_TASK_FINISHED, &CAutoMoveDlg::OnDriveTaskFinished)
	ON_BN_CLICKED(IDC_BTN_MAIN_ALL_START, &CAutoMoveDlg::OnBnClickedBtnMainAllStart)
	ON_BN_CLICKED(IDC_BTN_MAIN_ALL_STOP, &CAutoMoveDlg::OnBnClickedBtnMainAllStop)
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
	SetTrayIcon();
	m_pParam.Load();
	LoadAvailableDriveNames();
	CreateDriveUsageControls();
	UpdateFixedWindowSize();
	AlignControls();
	StartDriveUsageThread();
	EnsureDriveTaskWorkerStarted();

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
		m_driveTaskWorker.Stop();
		StopDriveUsageThread();
		EndDialog(IDOK);
	}
}


void CAutoMoveDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	const int nFixedHeight = GetFixedWindowHeight();

	lpMMI->ptMinTrackSize.x = 400; // 최소 가로
	lpMMI->ptMinTrackSize.y = nFixedHeight; // 최소 세로
	lpMMI->ptMaxTrackSize.x = 400; // 최대 가로
	lpMMI->ptMaxTrackSize.y = nFixedHeight; // 최대 세로

	CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CAutoMoveDlg::AlignControls()
{
	CRect rectClient;
	GetClientRect(&rectClient);
	const int cx = rectClient.Width();

	// 1. 컨트롤 가져오기
	CWnd* pBtnClose = GetDlgItem(IDC_BTN_MAIN_EXIT);
	CWnd* pBtnSetup = GetDlgItem(IDC_BTN_SETUP_OPEN);
	CWnd* pBtnAllStart = GetDlgItem(IDC_BTN_MAIN_ALL_START);
	CWnd* pBtnAllStop = GetDlgItem(IDC_BTN_MAIN_ALL_STOP);
	CWnd* pDriveUsageGroup = GetDlgItem(IDC_STATIC_MAIN_DRIVE_USAGE_TITLE);
	CWnd* pPic = GetDlgItem(IDC_STATIC_MAIN_LIST);

	const int LEFT_MARGIN = 7;
	const int RIGHT_MARGIN = 10; // 우측 끝단 마진
	const int BOTTOM_MARGIN = 10; // 하단 마진
	const int SECTION_GAP = 8;

	int nTopAreaBottom = 0;

	// 2. 우측 버튼만 X 위치를 맞추고, 크기와 Y 위치는 리소스 값을 그대로 사용합니다.
	if (pBtnSetup && pBtnSetup->GetSafeHwnd()) {
		CRect rSetup;
		pBtnSetup->GetWindowRect(&rSetup);
		ScreenToClient(&rSetup);
		pBtnSetup->MoveWindow(cx - rSetup.Width() - RIGHT_MARGIN, rSetup.top, rSetup.Width(), rSetup.Height());
		nTopAreaBottom = max(nTopAreaBottom, rSetup.bottom);
	}
	if (pBtnClose && pBtnClose->GetSafeHwnd()) {
		CRect rClose;
		pBtnClose->GetWindowRect(&rClose);
		ScreenToClient(&rClose);
		pBtnClose->MoveWindow(cx - rClose.Width() - RIGHT_MARGIN, rClose.top, rClose.Width(), rClose.Height());
		nTopAreaBottom = max(nTopAreaBottom, rClose.bottom);
	}
	if (pBtnAllStart && pBtnAllStart->GetSafeHwnd()) {
		CRect rAllStart;
		pBtnAllStart->GetWindowRect(&rAllStart);
		ScreenToClient(&rAllStart);
		nTopAreaBottom = max(nTopAreaBottom, rAllStart.bottom);
	}
	if (pBtnAllStop && pBtnAllStop->GetSafeHwnd()) {
		CRect rAllStop;
		pBtnAllStop->GetWindowRect(&rAllStop);
		ScreenToClient(&rAllStop);
		nTopAreaBottom = max(nTopAreaBottom, rAllStop.bottom);
	}

	// 3. 드라이브 용량 Progress Bar 영역 정렬
	int nListTop = nTopAreaBottom + SECTION_GAP;
	if (pDriveUsageGroup != nullptr && pDriveUsageGroup->GetSafeHwnd())
	{
		pDriveUsageGroup->ShowWindow(m_vecDriveUsageItems.empty() ? SW_HIDE : SW_SHOW);
	}

	if (!m_vecDriveUsageItems.empty())
	{
		const int nGroupLeft = LEFT_MARGIN;
		const int nGroupTop = nListTop;
		const int nGroupWidth = cx - LEFT_MARGIN - RIGHT_MARGIN;
		const int nRowLeft = nGroupLeft + DRIVE_USAGE_GROUP_INSET_X;
		const int nItemWidth = max(20, nGroupWidth - (DRIVE_USAGE_GROUP_INSET_X * 2));
		const int nGroupHeight = GetDriveUsageHeightDelta() - DRIVE_USAGE_BOTTOM_GAP;

		if (pDriveUsageGroup != nullptr && pDriveUsageGroup->GetSafeHwnd())
		{
			pDriveUsageGroup->MoveWindow(nGroupLeft, nGroupTop, nGroupWidth, nGroupHeight);
		}

		for (int i = 0; i < static_cast<int>(m_vecDriveUsageItems.size()); ++i)
		{
			CDriveUsageItem* pItem = m_vecDriveUsageItems[i];
			if (pItem != nullptr && pItem->GetSafeHwnd())
			{
				const int nTop = nGroupTop + DRIVE_USAGE_GROUP_TOP_PADDING + i * (DRIVE_USAGE_ITEM_HEIGHT + DRIVE_USAGE_ITEM_GAP);
				pItem->MoveWindow(nRowLeft, nTop, nItemWidth, DRIVE_USAGE_ITEM_HEIGHT);
			}
		}

		nListTop += nGroupHeight + DRIVE_USAGE_BOTTOM_GAP;
	}

	// 4. Picture Control 및 스크롤뷰 영역 업데이트
	if (pPic && pPic->GetSafeHwnd()) {
		CRect rPic;
		pPic->GetWindowRect(&rPic);
		ScreenToClient(&rPic);
		rPic.left = LEFT_MARGIN;
		rPic.top = nListTop;

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
	CSetupDlg dlg(this, &m_pParam);
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
	UNREFERENCED_PARAMETER(lParam);

	CPathItem* pItem = FindPathItemByHwnd(reinterpret_cast<HWND>(wParam));
	if (pItem != nullptr)
	{
		CParameter::PARAM_TEMPLATE* pTemplate = FindTemplateByName(pItem->m_strPathName);
		if (pTemplate != nullptr)
		{
			if (pItem->IsWaitingEvent() && !pItem->IsWorkingMoveCopy())
			{
				if (!ShouldTriggerDriveTask(*pTemplate, m_vecDriveInfos))
				{
					pItem->SetWaitingEvent(FALSE);
				}
				else if (!EnqueueDriveTask(*pTemplate, pItem)
					&& !m_driveTaskWorker.IsQueued(pTemplate->strName)
					&& !m_driveTaskWorker.IsWorking(pTemplate->strName))
				{
					pItem->SetWaitingEvent(FALSE);
				}
			}
			else if (!pItem->IsWaitingEvent() && !pItem->IsWorkingMoveCopy())
			{
				m_driveTaskWorker.Cancel(pTemplate->strName);
			}
		}
	}

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
	driveManager.CheckDriveUsage();
	m_vecAvailableDriveNames = driveManager.GetDriveNames();
	m_vecDriveInfos = driveManager.GetDriveInfos();
}

void CAutoMoveDlg::CreateDriveUsageControls()
{
	DestroyDriveUsageControls();

	for (int i = 0; i < static_cast<int>(m_vecDriveInfos.size()); ++i)
	{
		const DRIVE_INFO& driveInfo = m_vecDriveInfos[i];
		CDriveUsageItem* pItem = new CDriveUsageItem(this);
		if (!pItem->Create(IDD_DRIVEUSAGEITEM_DIALOG, this))
		{
			delete pItem;
			continue;
		}

		pItem->SetDlgCtrlID(DRIVE_USAGE_ITEM_ID_BASE + i);
		pItem->SetDriveInfo(driveInfo);
		pItem->ShowWindow(SW_SHOW);
		m_vecDriveUsageItems.push_back(pItem);
	}
}

void CAutoMoveDlg::DestroyDriveUsageControls()
{
	for (int i = 0; i < static_cast<int>(m_vecDriveUsageItems.size()); ++i)
	{
		CDriveUsageItem* pItem = m_vecDriveUsageItems[i];
		if (pItem != nullptr)
		{
			if (pItem->GetSafeHwnd())
			{
				pItem->DestroyWindow();
			}
			delete pItem;
		}
	}

	m_vecDriveUsageItems.clear();
}

void CAutoMoveDlg::UpdateDriveUsageControls(const std::vector<DRIVE_INFO>& vecDriveInfos)
{
	m_vecDriveInfos = vecDriveInfos;

	for (int i = 0; i < static_cast<int>(m_vecDriveUsageItems.size()); ++i)
	{
		CDriveUsageItem* pItem = m_vecDriveUsageItems[i];
		if (pItem == nullptr)
		{
			continue;
		}

		for (int j = 0; j < static_cast<int>(vecDriveInfos.size()); ++j)
		{
			const DRIVE_INFO& driveInfo = vecDriveInfos[j];
			if (pItem->GetDriveName().CompareNoCase(driveInfo.strDriveName) != 0)
			{
				continue;
			}

			pItem->SetDriveInfo(driveInfo);
			break;
		}
	}
}

int CAutoMoveDlg::GetDriveUsageHeightDelta() const
{
	if (m_vecDriveUsageItems.empty())
	{
		return 0;
	}

	const int nDriveCount = static_cast<int>(m_vecDriveUsageItems.size());
	const int nRowGapTotal = max(0, nDriveCount - 1) * DRIVE_USAGE_ITEM_GAP;
	return DRIVE_USAGE_GROUP_TOP_PADDING
		+ nDriveCount * DRIVE_USAGE_ITEM_HEIGHT
		+ nRowGapTotal
		+ DRIVE_USAGE_GROUP_BOTTOM_PADDING
		+ DRIVE_USAGE_BOTTOM_GAP;
}

int CAutoMoveDlg::GetFixedWindowHeight() const
{
	return 300 + GetDriveUsageHeightDelta();
}

void CAutoMoveDlg::UpdateFixedWindowSize()
{
	if (!GetSafeHwnd())
	{
		return;
	}

	CRect rectWindow;
	GetWindowRect(&rectWindow);
	SetWindowPos(nullptr, 0, 0, 400, GetFixedWindowHeight(), SWP_NOMOVE | SWP_NOZORDER);
}

LRESULT CAutoMoveDlg::OnDriveUsageUpdated(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);

	std::vector<DRIVE_INFO>* pDriveInfos = reinterpret_cast<std::vector<DRIVE_INFO>*>(lParam);
	if (pDriveInfos != nullptr)
	{
		UpdateDriveUsageControls(*pDriveInfos);
		EnqueueTriggeredDriveTasks(*pDriveInfos);
		delete pDriveInfos;
	}

	return 0;
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
	CAutoMoveDlg* pDlg = reinterpret_cast<CAutoMoveDlg*>(pParam);
	if (pDlg == nullptr)
	{
		return 0;
	}

	CDriveManager driveManager(pDlg->m_vecAvailableDriveNames);

	while (true)
	{
		if (WaitForSingleObject(pDlg->m_hDriveUsageStopEvent, 0) == WAIT_OBJECT_0)
		{
			return 0;
		}

		driveManager.CheckDriveUsage();
		std::vector<DRIVE_INFO>* pDriveInfos = new std::vector<DRIVE_INFO>(driveManager.GetDriveInfos());
		if (!pDlg->PostMessage(WM_DRIVE_USAGE_UPDATED, 0, reinterpret_cast<LPARAM>(pDriveInfos)))
		{
			delete pDriveInfos;
		}

		if (WaitForSingleObject(pDlg->m_hDriveUsageStopEvent, DRIVE_USAGE_CHECK_INTERVAL) == WAIT_OBJECT_0)
		{
			break;
		}
	}

	return 0;
}

BOOL CAutoMoveDlg::EnsureDriveTaskWorkerStarted()
{
	if (m_driveTaskWorker.IsRunning())
	{
		return TRUE;
	}

	return m_driveTaskWorker.Start(GetSafeHwnd());
}

CParameter::PARAM_TEMPLATE* CAutoMoveDlg::FindTemplateByName(LPCTSTR lpszTemplateName)
{
	if (lpszTemplateName == nullptr || *lpszTemplateName == _T('\0'))
	{
		return nullptr;
	}

	for (int i = 0; i < static_cast<int>(m_pParam.m_vecTemplate.size()); ++i)
	{
		if (m_pParam.m_vecTemplate[i].strName.CompareNoCase(lpszTemplateName) == 0)
		{
			return &m_pParam.m_vecTemplate[i];
		}
	}

	return nullptr;
}

const CParameter::PARAM_TEMPLATE* CAutoMoveDlg::FindTemplateByName(LPCTSTR lpszTemplateName) const
{
	return const_cast<CAutoMoveDlg*>(this)->FindTemplateByName(lpszTemplateName);
}

CPathItem* CAutoMoveDlg::FindPathItemByHwnd(HWND hPathItemWnd) const
{
	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd() || hPathItemWnd == nullptr)
	{
		return nullptr;
	}

	for (int i = 0; i < m_pScrollView->GetItemCount(); ++i)
	{
		CPathItem* pItem = dynamic_cast<CPathItem*>(m_pScrollView->GetItem(i));
		if (pItem != nullptr && pItem->GetSafeHwnd() == hPathItemWnd)
		{
			return pItem;
		}
	}

	return nullptr;
}

CPathItem* CAutoMoveDlg::FindPathItemByTemplateName(LPCTSTR lpszTemplateName) const
{
	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd()
		|| lpszTemplateName == nullptr || *lpszTemplateName == _T('\0'))
	{
		return nullptr;
	}

	for (int i = 0; i < m_pScrollView->GetItemCount(); ++i)
	{
		CPathItem* pItem = dynamic_cast<CPathItem*>(m_pScrollView->GetItem(i));
		if (pItem != nullptr && pItem->m_strPathName.CompareNoCase(lpszTemplateName) == 0)
		{
			return pItem;
		}
	}

	return nullptr;
}

BOOL CAutoMoveDlg::EnqueueDriveTask(CParameter::PARAM_TEMPLATE& paramTemplate, CPathItem* pPathItem)
{
	if (!EnsureDriveTaskWorkerStarted())
	{
		return FALSE;
	}

	const DRIVE_TASK task = BuildDriveTask(paramTemplate, pPathItem);
	if (!m_driveTaskWorker.Enqueue(task))
	{
		return FALSE;
	}

	if (CParameter::IsScheduleLimitMode(paramTemplate))
	{
		MarkScheduleTaskRunDate(paramTemplate);
	}

	return TRUE;
}

void CAutoMoveDlg::MarkScheduleTaskRunDate(CParameter::PARAM_TEMPLATE& paramTemplate)
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	paramTemplate.strLastScheduleRunDate = GetScheduleRunDate(now);
}

DRIVE_TASK CAutoMoveDlg::BuildDriveTask(const CParameter::PARAM_TEMPLATE& paramTemplate, CPathItem* pPathItem) const
{
	DRIVE_TASK task;
	task.hPathItemWnd = pPathItem != nullptr ? pPathItem->GetSafeHwnd() : nullptr;
	task.strTemplateName = paramTemplate.strName;
	task.strOriginPath = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::ORIGIN_PATH);
	task.strDestPath = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::DEST_PATH);
	task.strDriveName = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::DRIVE_NAME);
	task.nEndUsagePercent = _ttoi(CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::END_VALUE));
	task.eType = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::ENABLE_MOVE) == _T("1")
		? DRIVE_TASK_TYPE::MoveFiles
		: DRIVE_TASK_TYPE::RemoveFiles;
	return task;
}

void CAutoMoveDlg::ResetPathItemTaskState()
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
			pItem->SetWorkingMoveCopy(FALSE);
			pItem->SetWaitingEvent(FALSE);
		}
	}
}

int CAutoMoveDlg::EnqueueTriggeredDriveTasks(const std::vector<DRIVE_INFO>& vecDriveInfos)
{
	int nQueuedCount = 0;

	for (int i = 0; i < static_cast<int>(m_pParam.m_vecTemplate.size()); ++i)
	{
		CParameter::PARAM_TEMPLATE& paramTemplate = m_pParam.m_vecTemplate[i];
		if (!ShouldTriggerDriveTask(paramTemplate, vecDriveInfos))
		{
			continue;
		}

		CPathItem* pItem = FindPathItemByTemplateName(paramTemplate.strName);
		if (EnqueueDriveTask(paramTemplate, pItem))
		{
			++nQueuedCount;
			if (pItem != nullptr)
			{
				pItem->SetWaitingEvent(TRUE);
			}
		}
	}

	return nQueuedCount;
}

BOOL CAutoMoveDlg::ShouldTriggerDriveTask(const CParameter::PARAM_TEMPLATE& paramTemplate, const std::vector<DRIVE_INFO>& vecDriveInfos) const
{
	const CString strLimitMode = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::LIMIT_MODE);
	if (strLimitMode == CParameter::TemplateKey::LIMIT_MODE_SCHEDULE)
	{
		SYSTEMTIME now;
		GetLocalTime(&now);
		return ShouldTriggerScheduleTask(paramTemplate, now);
	}

	const CString strDriveName = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::DRIVE_NAME);
	const int nUsagePercent = CDriveManager::FindDriveUsagePercent(vecDriveInfos, strDriveName);
	if (nUsagePercent < 0)
	{
		return FALSE;
	}

	const int nLimitValue = _ttoi(CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::LIMIT_VALUE));
	const int nEndValue = _ttoi(CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::END_VALUE));
	return nLimitValue > 0
		&& nEndValue > 0
		&& nUsagePercent >= nLimitValue
		&& nUsagePercent > nEndValue;
}

BOOL CAutoMoveDlg::ShouldTriggerScheduleTask(const CParameter::PARAM_TEMPLATE& paramTemplate, const SYSTEMTIME& now) const
{
	const CString strToday = GetScheduleRunDate(now);
	if (!paramTemplate.strLastScheduleRunDate.IsEmpty()
		&& paramTemplate.strLastScheduleRunDate == strToday)
	{
		return FALSE;
	}

	const CString strScheduleDays = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::SCHEDULE_DAYS);
	if (!IsScheduleDayMatched(strScheduleDays, now.wDayOfWeek))
	{
		return FALSE;
	}

	const CString strScheduleTime = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::SCHEDULE_TIME);
	if (strScheduleTime.GetLength() != 4)
	{
		return FALSE;
	}

	const int nScheduleHour = _ttoi(strScheduleTime.Left(2));
	const int nScheduleMinute = _ttoi(strScheduleTime.Mid(2, 2));
	if (nScheduleHour < 0 || nScheduleHour > 23 || nScheduleMinute < 0 || nScheduleMinute > 59)
	{
		return FALSE;
	}

	const int nNowMinutes = static_cast<int>(now.wHour) * 60 + static_cast<int>(now.wMinute);
	const int nScheduleMinutes = nScheduleHour * 60 + nScheduleMinute;
	return nNowMinutes >= nScheduleMinutes;
}

BOOL CAutoMoveDlg::IsScheduleDayMatched(const CString& strScheduleDays, WORD wDayOfWeek) const
{
	CString strDays = strScheduleDays;
	strDays.Trim();
	if (strDays.IsEmpty())
	{
		return FALSE;
	}

	static constexpr LPCTSTR arrKoreanDays[] = {
		_T("일"), _T("월"), _T("화"), _T("수"), _T("목"), _T("금"), _T("토")
	};
	static constexpr LPCTSTR arrEnglishDays[] = {
		_T("Sun"), _T("Mon"), _T("Tue"), _T("Wed"), _T("Thu"), _T("Fri"), _T("Sat")
	};

	if (strDays.Find(_T("매일")) >= 0
		|| strDays.CompareNoCase(_T("Daily")) == 0
		|| strDays.CompareNoCase(_T("Everyday")) == 0
		|| strDays.Find(_T("All")) >= 0)
	{
		return TRUE;
	}

	if (wDayOfWeek <= 6)
	{
		CString strNumber;
		strNumber.Format(_T("%u"), wDayOfWeek);
		return strDays.Find(arrKoreanDays[wDayOfWeek]) >= 0
			|| strDays.Find(arrEnglishDays[wDayOfWeek]) >= 0
			|| strDays.Find(strNumber) >= 0;
	}

	return FALSE;
}

CString CAutoMoveDlg::GetScheduleRunDate(const SYSTEMTIME& time) const
{
	CString strDate;
	strDate.Format(_T("%04u%02u%02u"), time.wYear, time.wMonth, time.wDay);
	return strDate;
}

LRESULT CAutoMoveDlg::OnDriveTaskStarted(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);

	DRIVE_TASK_NOTIFY* pNotify = reinterpret_cast<DRIVE_TASK_NOTIFY*>(lParam);
	if (pNotify != nullptr)
	{
		CPathItem* pItem = FindPathItemByHwnd(pNotify->hPathItemWnd);
		if (pItem == nullptr)
		{
			pItem = FindPathItemByTemplateName(pNotify->strTemplateName);
		}

		if (pItem != nullptr)
		{
			pItem->SetWorkingMoveCopy(TRUE);
			pItem->SetWaitingEvent(FALSE);
		}

		delete pNotify;
	}

	UpdatePathItemBlinkTimer();
	return 0;
}

LRESULT CAutoMoveDlg::OnDriveTaskFinished(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);

	DRIVE_TASK_NOTIFY* pNotify = reinterpret_cast<DRIVE_TASK_NOTIFY*>(lParam);
	if (pNotify != nullptr)
	{
		CPathItem* pItem = FindPathItemByHwnd(pNotify->hPathItemWnd);
		if (pItem == nullptr)
		{
			pItem = FindPathItemByTemplateName(pNotify->strTemplateName);
		}

		if (pItem != nullptr)
		{
			pItem->SetWorkingMoveCopy(FALSE);
			pItem->SetWaitingEvent(FALSE);
		}

		delete pNotify;
	}

	UpdatePathItemBlinkTimer();
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

void CAutoMoveDlg::OnBnClickedBtnMainAllStart()
{
	if (!EnsureDriveTaskWorkerStarted())
	{
		MessageBox(_T("작업 스레드를 시작할 수 없습니다."),
			_T("작업 실행"), MB_OK | MB_ICONERROR);
		return;
	}

	const int nQueuedCount = EnqueueTriggeredDriveTasks(m_vecDriveInfos);

	CString strMessage;
	strMessage.Format(_T("실행 조건을 만족해 대기열에 등록된 작업 수: %d"), nQueuedCount);
	MessageBox(strMessage, _T("작업 실행"), MB_OK | MB_ICONINFORMATION);
}

void CAutoMoveDlg::OnBnClickedBtnMainAllStop()
{
	m_driveTaskWorker.Stop();
	ResetPathItemTaskState();
	EnsureDriveTaskWorkerStarted();
}

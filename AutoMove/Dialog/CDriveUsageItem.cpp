#include "pch.h"
#include "../AutoMove.h"
#include "CDriveUsageItem.h"

IMPLEMENT_DYNAMIC(CDriveUsageItem, CDialogEx)

BEGIN_MESSAGE_MAP(CDriveUsageItem, CDialogEx)
	ON_WM_SIZE()
END_MESSAGE_MAP()

CDriveUsageItem::CDriveUsageItem(CWnd* pParent)
	: CDialogEx(IDD_DRIVEUSAGEITEM_DIALOG, pParent)
	, m_nUsagePercent(0)
{
}

CDriveUsageItem::~CDriveUsageItem()
{
}

void CDriveUsageItem::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BOOL CDriveUsageItem::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	CProgressCtrl* pProgress = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS_DRIVEUSAGE);
	if (pProgress != nullptr && pProgress->GetSafeHwnd())
	{
		pProgress->SetRange(0, 100);
		pProgress->SetBarColor(RGB(69, 132, 245));
		pProgress->SetBkColor(RGB(238, 242, 247));
	}

	AlignControls();
	return TRUE;
}

void CDriveUsageItem::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	AlignControls();
}

void CDriveUsageItem::SetDriveInfo(const DRIVE_INFO& driveInfo)
{
	m_strDriveName = driveInfo.strDriveName;
	m_nUsagePercent = driveInfo.nUsagePercent;

	CString strDriveName;
	strDriveName.Format(_T("%s"), static_cast<LPCTSTR>(m_strDriveName));
	SetDlgItemText(IDC_STATIC_DRIVEUSAGE_NAME, strDriveName);

	CProgressCtrl* pProgress = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS_DRIVEUSAGE);
	if (pProgress != nullptr && pProgress->GetSafeHwnd())
	{
		pProgress->SetPos(m_nUsagePercent);
	}

	CString strPercent;
	strPercent.Format(_T("%d%%"), m_nUsagePercent);
	SetDlgItemText(IDC_STATIC_DRIVEUSAGE_PERCENT, strPercent);
}

CString CDriveUsageItem::GetDriveName() const
{
	return m_strDriveName;
}

void CDriveUsageItem::AlignControls()
{
	if (!GetSafeHwnd())
	{
		return;
	}

	CRect rectClient;
	GetClientRect(&rectClient);

	const int nGap = 5;
	const int nLabelWidth = 42;
	const int nPercentWidth = 34;
	const int nProgressLeft = nLabelWidth;
	const int nProgressWidth = max(20, rectClient.Width() - nLabelWidth - nPercentWidth - nGap);

	CWnd* pName = GetDlgItem(IDC_STATIC_DRIVEUSAGE_NAME);
	CWnd* pProgress = GetDlgItem(IDC_PROGRESS_DRIVEUSAGE);
	CWnd* pPercent = GetDlgItem(IDC_STATIC_DRIVEUSAGE_PERCENT);

	if (pName != nullptr && pName->GetSafeHwnd())
	{
		pName->MoveWindow(0, 2, nLabelWidth - nGap, rectClient.Height());
	}
	if (pProgress != nullptr && pProgress->GetSafeHwnd())
	{
		pProgress->MoveWindow(nProgressLeft, 0, nProgressWidth, rectClient.Height());
	}
	if (pPercent != nullptr && pPercent->GetSafeHwnd())
	{
		pPercent->MoveWindow(nProgressLeft + nProgressWidth + nGap, 2, nPercentWidth, rectClient.Height());
	}
}

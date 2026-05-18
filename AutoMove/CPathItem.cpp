// CPathItem.cpp: 구현 파일
//

#include "pch.h"
#include "resource.h"
#include "afxdialogex.h"
#include "CPathItem.h"

namespace
{
	CString FormatScheduleTime(const CString& strScheduleTime)
	{
		if (strScheduleTime.GetLength() == 4)
		{
			CString strTime;
			strTime.Format(_T("%s:%s"), static_cast<LPCTSTR>(strScheduleTime.Left(2)),
				static_cast<LPCTSTR>(strScheduleTime.Mid(2, 2)));
			return strTime;
		}

		return strScheduleTime;
	}

	CString FormatTemplateEventText(const CParameter::PARAM_TEMPLATE& paramTemplate)
	{
		const CString strLimitMode = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::LIMIT_MODE);
		if (strLimitMode == CParameter::TemplateKey::LIMIT_MODE_SCHEDULE)
		{
			CString strEvent;
			strEvent.Format(_T("%s %s 이후"),
				static_cast<LPCTSTR>(CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::SCHEDULE_DAYS)),
				static_cast<LPCTSTR>(FormatScheduleTime(CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::SCHEDULE_TIME))));
			return strEvent;
		}

		CString strEvent;
		strEvent.Format(_T("%s의 용량이 %s%% 이상"),
			static_cast<LPCTSTR>(CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::DRIVE_NAME)),
			static_cast<LPCTSTR>(CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::LIMIT_VALUE)));
		return strEvent;
	}
}


// CPathItem 대화 상자

IMPLEMENT_DYNAMIC(CPathItem, CDialogEx)

CPathItem::CPathItem(CWnd* pParent, CString strPathName)
	: CDialogEx(IDD_PATHITEM_DIALOG, pParent)
	, m_bWaitingEvent(FALSE)
	, m_bWorkingMoveCopy(FALSE)
	, m_bBlinkOn(TRUE)
{
	m_strPathName = strPathName;
}

CPathItem::~CPathItem()
{
}

BOOL CPathItem::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	CWnd* pAct = GetDlgItem(IDC_STATIC_PATHITEM_ACT);
	if (pAct != nullptr && pAct->GetSafeHwnd())
	{
		pAct->ModifyStyle(SS_TYPEMASK, SS_OWNERDRAW);
	}

	SetPathName(m_strPathName);
	RefreshActControl();
	UpdateButtons();
	return TRUE;
}

void CPathItem::LoadFromTemplate(const CParameter::PARAM_TEMPLATE& paramTemplate)
{
	SetPathName(paramTemplate.strName);
	SetEventText(FormatTemplateEventText(paramTemplate));
	SetWaitingEvent(CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::BOOT_START) == _T("1"));
	RefreshActControl();
}

void CPathItem::SetPathName(LPCTSTR lpszPathName)
{
	m_strPathName = lpszPathName;

	CWnd* pName = GetDlgItem(IDC_STATIC_PATHITEM_NAME);
	if (pName != nullptr && pName->GetSafeHwnd())
	{
		pName->SetWindowText(m_strPathName);
	}
}

void CPathItem::SetEventText(LPCTSTR lpszEventText)
{
	CString strEventText;
	strEventText.Format(_T("이벤트: %s"), lpszEventText);

	CWnd* pEvent = GetDlgItem(IDC_STATIC_PATHITEM_EVENT);
	if (pEvent != nullptr && pEvent->GetSafeHwnd())
	{
		pEvent->SetWindowText(strEventText);
	}
}

void CPathItem::SetWaitingEvent(BOOL bWaitingEvent)
{
	if (m_bWaitingEvent == bWaitingEvent)
	{
		return;
	}

	m_bWaitingEvent = bWaitingEvent;
	RefreshActControl();
	UpdateButtons();
	NotifyStateChanged();
}

void CPathItem::SetWorkingMoveCopy(BOOL bWorkingMoveCopy)
{
	if (m_bWorkingMoveCopy == bWorkingMoveCopy)
	{
		return;
	}

	m_bWorkingMoveCopy = bWorkingMoveCopy;
	RefreshActControl();
	UpdateButtons();
	NotifyStateChanged();
}

BOOL CPathItem::IsWaitingEvent() const
{
	return m_bWaitingEvent;
}

BOOL CPathItem::IsWorkingMoveCopy() const
{
	return m_bWorkingMoveCopy;
}

void CPathItem::SetBlinkOn(BOOL bBlinkOn)
{
	if (m_bBlinkOn == bBlinkOn)
	{
		return;
	}

	m_bBlinkOn = bBlinkOn;
	RefreshActControl();
}

void CPathItem::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CPathItem, CDialogEx)
	ON_WM_MOUSEWHEEL()
    ON_WM_SIZE()
	ON_WM_DRAWITEM()
	ON_BN_CLICKED(IDC_BTN_PATHITEM_START, &CPathItem::OnBnClickedPathitemStart)
	ON_BN_CLICKED(IDC_BTN_PATHITEM_STOP, &CPathItem::OnBnClickedPathitemStop)
END_MESSAGE_MAP()


BOOL CPathItem::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// 현재 아이템에서 마우스 휠 이벤트 부모에게 전달
	CWnd* pParent = GetParent();
	if (pParent == nullptr)
	{
		return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
	}

	return pParent->SendMessage(WM_MOUSEWHEEL, MAKEWPARAM(nFlags, zDelta), MAKELPARAM(pt.x, pt.y)) != 0;
}

void CPathItem::OnBnClickedPathitemStart()
{
	SetWaitingEvent(TRUE);
}

void CPathItem::OnBnClickedPathitemStop()
{
	if (m_bWorkingMoveCopy)
	{
		const int nResult = MessageBox(_T("현재 진행중인 작업을 취소하시겠습니까?"),
			_T("작업 취소 확인"), MB_YESNO | MB_ICONQUESTION);
		if (nResult != IDYES)
		{
			return;
		}
	}

	SetWorkingMoveCopy(FALSE);
	SetWaitingEvent(FALSE);
}

void CPathItem::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);

    if (cx <= 0) return;

	CWnd* pBtnStop = GetDlgItem(IDC_BTN_PATHITEM_STOP);
	CWnd* pBtnStart = GetDlgItem(IDC_BTN_PATHITEM_START);
	CWnd* pName = GetDlgItem(IDC_STATIC_PATHITEM_NAME);
	CWnd* pEvent = GetDlgItem(IDC_STATIC_PATHITEM_EVENT);
	if (pBtnStop == nullptr || !pBtnStop->GetSafeHwnd()
		|| pBtnStart == nullptr || !pBtnStart->GetSafeHwnd())
	{
		return;
	}

	const int nRightMargin = 7;
	const int nGap = 5;

	CRect rectStop;
	CRect rectStart;
	pBtnStop->GetWindowRect(&rectStop);
	pBtnStart->GetWindowRect(&rectStart);
	ScreenToClient(&rectStop);
	ScreenToClient(&rectStart);

	const int nStopWidth = rectStop.Width();
	const int nStopHeight = rectStop.Height();
	const int nStartWidth = rectStart.Width();
	const int nStartHeight = rectStart.Height();
	const int nStopLeft = max(0, cx - nRightMargin - nStopWidth);
	const int nStopTop = max(0, (cy - nStopHeight) / 2);
	const int nStartLeft = max(0, nStopLeft - nGap - nStartWidth);
	const int nStartTop = max(0, (cy - nStartHeight) / 2);

	pBtnStop->MoveWindow(nStopLeft, nStopTop, nStopWidth, nStopHeight);
	pBtnStart->MoveWindow(nStartLeft, nStartTop, nStartWidth, nStartHeight);

	const int nTextRight = max(0, nStartLeft - nGap);
	if (pName != nullptr && pName->GetSafeHwnd())
	{
		CRect rectName;
		pName->GetWindowRect(&rectName);
		ScreenToClient(&rectName);
		pName->MoveWindow(rectName.left, rectName.top, max(0, nTextRight - rectName.left), rectName.Height());
	}

	if (pEvent != nullptr && pEvent->GetSafeHwnd())
	{
		CRect rectEvent;
		pEvent->GetWindowRect(&rectEvent);
		ScreenToClient(&rectEvent);
		pEvent->MoveWindow(rectEvent.left, rectEvent.top, max(0, nTextRight - rectEvent.left), rectEvent.Height());
	}
}

void CPathItem::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl == IDC_STATIC_PATHITEM_ACT && lpDrawItemStruct != nullptr)
	{
		CDC dc;
		dc.Attach(lpDrawItemStruct->hDC);

		CRect rect(lpDrawItemStruct->rcItem);
		dc.FillSolidRect(rect, GetActColor());
		dc.DrawEdge(rect, EDGE_SUNKEN, BF_RECT);

		dc.Detach();
		return;
	}

	CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
}

COLORREF CPathItem::GetActColor() const
{
	if (m_bWorkingMoveCopy)
	{
		if (!m_bBlinkOn)
		{
			return RGB(235, 235, 235);
		}

		return RGB(0, 120, 215);
	}

	if (m_bWaitingEvent)
	{
		if (!m_bBlinkOn)
		{
			return RGB(235, 235, 235);
		}

		return RGB(0, 180, 80);
	}

	return RGB(220, 40, 40);
}

void CPathItem::RefreshActControl()
{
	CWnd* pAct = GetDlgItem(IDC_STATIC_PATHITEM_ACT);
	if (pAct != nullptr && pAct->GetSafeHwnd())
	{
		pAct->Invalidate();
		pAct->UpdateWindow();
	}
}

void CPathItem::UpdateButtons()
{
	CWnd* pBtnStart = GetDlgItem(IDC_BTN_PATHITEM_START);
	CWnd* pBtnStop = GetDlgItem(IDC_BTN_PATHITEM_STOP);

	if (pBtnStart != nullptr && pBtnStart->GetSafeHwnd())
	{
		pBtnStart->EnableWindow(!m_bWaitingEvent && !m_bWorkingMoveCopy);
	}

	if (pBtnStop != nullptr && pBtnStop->GetSafeHwnd())
	{
		pBtnStop->EnableWindow(m_bWaitingEvent || m_bWorkingMoveCopy);
	}
}

void CPathItem::NotifyStateChanged()
{
	CWnd* pParent = GetParent();
	if (pParent != nullptr && pParent->GetSafeHwnd())
	{
		CWnd* pOwner = pParent->GetParent();
		if (pOwner != nullptr && pOwner->GetSafeHwnd())
		{
			pOwner->PostMessage(WM_PATHITEM_STATE_CHANGED, reinterpret_cast<WPARAM>(GetSafeHwnd()), 0);
		}
	}
}

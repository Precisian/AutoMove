// CSetupItem.cpp: 구현 파일
//

#include "pch.h"
#include "AutoMove.h"
#include "afxdialogex.h"
#include "CSetupItem.h"
#include "CListScrollView.h"


// CSetupItem 대화 상자

IMPLEMENT_DYNAMIC(CSetupItem, CDialogEx)

CSetupItem::CSetupItem(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SETUPITEM_DIALOG, pParent)
{
}

CSetupItem::~CSetupItem()
{
}

void CSetupItem::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CSetupItem, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_RADIO_SETUPITEM_LIMIT_STORAGE, &CSetupItem::OnBnClickedRadioLimitStorage)
	ON_BN_CLICKED(IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE, &CSetupItem::OnBnClickedRadioLimitSchedule)
	ON_BN_CLICKED(IDC_BTN_SETUPITEM_REMOVE, &CSetupItem::OnBnClickedBtnSetupitemRemove)
END_MESSAGE_MAP()


BOOL CSetupItem::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	AlignControls();
	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME);
	if (pEdit)
	{
		pEdit->SetCueBanner(_T("ex)0800"));
	}

	if (!IsDlgButtonChecked(IDC_RADIO_SETUPITEM_LIMIT_STORAGE)
		&& !IsDlgButtonChecked(IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE))
	{
		CheckRadioButton(IDC_RADIO_SETUPITEM_LIMIT_STORAGE, IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE,
			IDC_RADIO_SETUPITEM_LIMIT_STORAGE);
	}

	UpdateLimitControls();
	return TRUE;
}

void CSetupItem::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	AlignControls();
}

void CSetupItem::AlignControls()
{
	if (!GetSafeHwnd())
	{
		return;
	}

	CRect rectClient;
	GetClientRect(&rectClient);
	int cx = rectClient.Width();

	if (cx <= 0)
	{
		return;
	}

	CWnd* pBtnRemove = GetDlgItem(IDC_BTN_SETUPITEM_REMOVE);
	CWnd* pEditOrigin = GetDlgItem(IDC_EDIT_SETUPITEM_PATH_ORIGIN);
	CWnd* pEditDest = GetDlgItem(IDC_EDIT_SETUPITEM_PATH_DEST);
	CWnd* pGroupStorage = FindGroupBox(_T("용량 제한"));
	CWnd* pGroupSchedule = FindGroupBox(_T("스케줄"));
	CWnd* pComboScheduleDays = GetDlgItem(IDC_COMBO_SETUPITEM_LIMIT_SCHEJULE_DAYS);
	CWnd* pEditScheduleTime = GetDlgItem(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME);
	CWnd* pTextScheduleAfter = FindChildByText(_T("이후에 실행"));

	if (pBtnRemove == nullptr || !pBtnRemove->GetSafeHwnd())
	{
		return;
	}

	const int nRightMargin = 7;
	const int nGap = 5;

	CRect rectRemove;
	pBtnRemove->GetWindowRect(&rectRemove);
	ScreenToClient(&rectRemove);

	const int nRemoveWidth = rectRemove.Width();
	const int nRemoveRight = cx - nRightMargin;
	const int nRemoveLeft = max(0, nRemoveRight - nRemoveWidth);

	pBtnRemove->MoveWindow(nRemoveLeft, rectRemove.top, nRemoveWidth, rectRemove.Height());

	const int nEditRight = nRemoveRight;
	CWnd* arrEdit[] = { pEditOrigin, pEditDest };
	for (int i = 0; i < 2; ++i)
	{
		CWnd* pEdit = arrEdit[i];
		if (pEdit == nullptr || !pEdit->GetSafeHwnd())
		{
			continue;
		}

		CRect rectEdit;
		pEdit->GetWindowRect(&rectEdit);
		ScreenToClient(&rectEdit);

		pEdit->MoveWindow(rectEdit.left, rectEdit.top, max(0, nEditRight - rectEdit.left), rectEdit.Height());
	}

	if (pGroupStorage != nullptr && pGroupStorage->GetSafeHwnd()
		&& pGroupSchedule != nullptr && pGroupSchedule->GetSafeHwnd())
	{
		const int nGroupGap = 5;

		CRect rectStorage;
		CRect rectSchedule;
		pGroupStorage->GetWindowRect(&rectStorage);
		pGroupSchedule->GetWindowRect(&rectSchedule);
		ScreenToClient(&rectStorage);
		ScreenToClient(&rectSchedule);

		const int nTotalLeft = rectStorage.left;
		const int nTotalRight = nEditRight;
		const int nTotalWidth = max(0, nTotalRight - nTotalLeft - nGroupGap);
		const int nStorageWidth = nTotalWidth / 2;
		const int nScheduleLeft = nTotalLeft + nStorageWidth + nGroupGap;
		const int nScheduleWidth = max(0, nTotalRight - nScheduleLeft);
		const int nScheduleOffsetX = nScheduleLeft - rectSchedule.left;

		pGroupStorage->MoveWindow(rectStorage.left, rectStorage.top, nStorageWidth, rectStorage.Height());
		pGroupSchedule->MoveWindow(nScheduleLeft, rectSchedule.top, nScheduleWidth, rectSchedule.Height());

		CWnd* arrScheduleChild[] = { pComboScheduleDays, pEditScheduleTime, pTextScheduleAfter };
		for (int i = 0; i < 3; ++i)
		{
			CWnd* pChild = arrScheduleChild[i];
			if (pChild == nullptr || !pChild->GetSafeHwnd())
			{
				continue;
			}

			CRect rectChild;
			pChild->GetWindowRect(&rectChild);
			ScreenToClient(&rectChild);

			pChild->MoveWindow(rectChild.left + nScheduleOffsetX, rectChild.top, rectChild.Width(), rectChild.Height());
		}
	}
}

void CSetupItem::OnBnClickedRadioLimitStorage()
{
	UpdateLimitControls();
}

void CSetupItem::OnBnClickedRadioLimitSchedule()
{
	UpdateLimitControls();
}

void CSetupItem::UpdateLimitControls()
{
	const BOOL bStorageMode = IsDlgButtonChecked(IDC_RADIO_SETUPITEM_LIMIT_STORAGE) == BST_CHECKED;
	const BOOL bScheduleMode = IsDlgButtonChecked(IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE) == BST_CHECKED;

	CWnd* pEditStorageValue = GetDlgItem(IDC_EDIT_SETUPITEM_LIMIT_VALUE);
	CWnd* pComboScheduleDays = GetDlgItem(IDC_COMBO_SETUPITEM_LIMIT_SCHEJULE_DAYS);
	CWnd* pEditScheduleTime = GetDlgItem(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME);

	if (pEditStorageValue != nullptr && pEditStorageValue->GetSafeHwnd())
	{
		pEditStorageValue->EnableWindow(bStorageMode);
	}

	if (pComboScheduleDays != nullptr && pComboScheduleDays->GetSafeHwnd())
	{
		pComboScheduleDays->EnableWindow(bScheduleMode);
	}

	if (pEditScheduleTime != nullptr && pEditScheduleTime->GetSafeHwnd())
	{
		pEditScheduleTime->EnableWindow(bScheduleMode);
	}
}

CWnd* CSetupItem::FindGroupBox(LPCTSTR lpszText)
{
	CWnd* pChild = GetWindow(GW_CHILD);
	while (pChild != nullptr)
	{
		CString strText;
		pChild->GetWindowText(strText);

		const LONG_PTR nStyle = pChild->GetStyle();
		if ((nStyle & BS_TYPEMASK) == BS_GROUPBOX && strText == lpszText)
		{
			return pChild;
		}

		pChild = pChild->GetWindow(GW_HWNDNEXT);
	}

	return nullptr;
}

CWnd* CSetupItem::FindChildByText(LPCTSTR lpszText)
{
	CWnd* pChild = GetWindow(GW_CHILD);
	while (pChild != nullptr)
	{
		CString strText;
		pChild->GetWindowText(strText);

		if (strText == lpszText)
		{
			return pChild;
		}

		pChild = pChild->GetWindow(GW_HWNDNEXT);
	}

	return nullptr;
}

void CSetupItem::OnBnClickedBtnSetupitemRemove()
{
	CWnd* pParent = GetParent();
	if (pParent == nullptr || !pParent->GetSafeHwnd())
	{
		return;
	}

	pParent->PostMessage(WM_LISTSCROLL_REMOVE_ITEM, reinterpret_cast<WPARAM>(GetSafeHwnd()), 0);
}

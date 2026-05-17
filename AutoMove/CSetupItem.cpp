// CSetupItem.cpp: 구현 파일
//

#include "pch.h"
#include "AutoMove.h"
#include "afxdialogex.h"
#include "CSetupItem.h"
#include "CListScrollView.h"

namespace
{
	constexpr LPCTSTR KEY_NAME = _T("Name");
	constexpr LPCTSTR KEY_ORIGIN_PATH = _T("OriginPath");
	constexpr LPCTSTR KEY_DEST_PATH = _T("DestPath");
	constexpr LPCTSTR KEY_ENABLE_MOVE = _T("EnableMove");
	constexpr LPCTSTR KEY_BOOT_START = _T("BootStart");
	constexpr LPCTSTR KEY_DRIVE_NAME = _T("DriveName");
	constexpr LPCTSTR KEY_LIMIT_MODE = _T("LimitMode");
	constexpr LPCTSTR KEY_LIMIT_VALUE = _T("LimitValue");
	constexpr LPCTSTR KEY_END_VALUE = _T("EndValue");
	constexpr LPCTSTR KEY_SCHEDULE_DAYS = _T("ScheduleDays");
	constexpr LPCTSTR KEY_SCHEDULE_TIME = _T("ScheduleTime");
	constexpr LPCTSTR LIMIT_MODE_STORAGE = _T("Storage");
	constexpr LPCTSTR LIMIT_MODE_SCHEDULE = _T("Schedule");

	CString FindTemplateValue(const CParameter::PARAM_TEMPLATE& paramTemplate, LPCTSTR lpszKey, LPCTSTR lpszDefault = _T(""))
	{
		for (int i = 0; i < static_cast<int>(paramTemplate.vecValue.size()); ++i)
		{
			if (paramTemplate.vecValue[i].strKey == lpszKey)
			{
				return paramTemplate.vecValue[i].strValue;
			}
		}

		return lpszDefault;
	}

	void AddTemplateValue(CParameter::PARAM_TEMPLATE& paramTemplate, LPCTSTR lpszKey, const CString& strValue)
	{
		CParameter::PARAM_TEMPLATE_VALUE value;
		value.strKey = lpszKey;
		value.strValue = strValue;
		paramTemplate.vecValue.push_back(value);
	}
}


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

void CSetupItem::SetBootStart(BOOL bBootStart)
{
	CheckDlgButton(IDC_CK_SETUPITEM_BOOTSTART, bBootStart ? BST_CHECKED : BST_UNCHECKED);
}

BOOL CSetupItem::IsBootStart() const
{
	return IsDlgButtonChecked(IDC_CK_SETUPITEM_BOOTSTART) == BST_CHECKED;
}

BOOL CSetupItem::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg != nullptr && pMsg->message == WM_KEYDOWN
		&& (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
	{
		return TRUE;
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CSetupItem::OnOK()
{
}

void CSetupItem::OnCancel()
{
}


BEGIN_MESSAGE_MAP(CSetupItem, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_CK_SETUPITEM_ENABLEMOVE, &CSetupItem::OnBnClickedCheckEnableMove)
	ON_BN_CLICKED(IDC_RADIO_SETUPITEM_LIMIT_STORAGE, &CSetupItem::OnBnClickedRadioLimitStorage)
	ON_BN_CLICKED(IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE, &CSetupItem::OnBnClickedRadioLimitSchedule)
	ON_BN_CLICKED(IDC_BTN_SETUPITEM_REMOVE, &CSetupItem::OnBnClickedBtnSetupitemRemove)
	ON_EN_CHANGE(IDC_EDIT_SETUPITEM_LIMIT_VALUE, &CSetupItem::OnEnChangeEditLimitValue)
	ON_EN_CHANGE(IDC_EDIT_SETUPITEM_END_VALUE, &CSetupItem::OnEnChangeEditEndValue)
	ON_EN_CHANGE(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME, &CSetupItem::OnEnChangeEditScheduleTime)
END_MESSAGE_MAP()


BOOL CSetupItem::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	AlignControls();
	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME);
	if (pEdit)
	{
		pEdit->SetLimitText(4);
		pEdit->SetCueBanner(_T("ex)0800"));
	}

	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_SETUPITEM_LIMIT_VALUE);
	if (pEdit)
	{
		pEdit->SetLimitText(3);
	}

	pEdit = (CEdit*)GetDlgItem(IDC_EDIT_SETUPITEM_END_VALUE);
	if (pEdit)
	{
		pEdit->SetLimitText(3);
	}

	if (!IsDlgButtonChecked(IDC_RADIO_SETUPITEM_LIMIT_STORAGE)
		&& !IsDlgButtonChecked(IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE))
	{
		CheckRadioButton(IDC_RADIO_SETUPITEM_LIMIT_STORAGE, IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE,
			IDC_RADIO_SETUPITEM_LIMIT_STORAGE);
	}

	UpdateLimitControls();
	UpdateMoveControls();
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
	CWnd* pGroupEnd = FindGroupBox(_T("종료조건"));
	CWnd* pComboScheduleDays = GetDlgItem(IDC_COMBO_SETUPITEM_LIMIT_SCHEJULE_DAYS);
	CWnd* pEditScheduleTime = GetDlgItem(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME);
	CWnd* pTextScheduleAfter = FindChildByText(_T("이후에 실행"));
	CWnd* pEditEndValue = GetDlgItem(IDC_EDIT_SETUPITEM_END_VALUE);
	CWnd* pTextEndAfter = FindChildByText(_T("% 미만 시 종료"));

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
		&& pGroupSchedule != nullptr && pGroupSchedule->GetSafeHwnd()
		&& pGroupEnd != nullptr && pGroupEnd->GetSafeHwnd())
	{
		const int nGroupGap = 3;

		CRect rectStorage;
		CRect rectSchedule;
		CRect rectEnd;
		pGroupStorage->GetWindowRect(&rectStorage);
		pGroupSchedule->GetWindowRect(&rectSchedule);
		pGroupEnd->GetWindowRect(&rectEnd);
		ScreenToClient(&rectStorage);
		ScreenToClient(&rectSchedule);
		ScreenToClient(&rectEnd);

		const int nTotalLeft = rectStorage.left;
		const int nTotalRight = nEditRight;
		const int nTotalWidth = max(0, nTotalRight - nTotalLeft - (nGroupGap * 2));
		const int nStorageWidth = nTotalWidth * 91 / (91 + 127 + 96);
		const int nScheduleLeft = nTotalLeft + nStorageWidth + nGroupGap;
		const int nScheduleWidth = nTotalWidth * 127 / (91 + 127 + 96);
		const int nEndLeft = nScheduleLeft + nScheduleWidth + nGroupGap;
		const int nEndWidth = max(0, nTotalRight - nEndLeft);
		const int nScheduleOffsetX = nScheduleLeft - rectSchedule.left;
		const int nEndOffsetX = nEndLeft - rectEnd.left;

		pGroupStorage->MoveWindow(rectStorage.left, rectStorage.top, nStorageWidth, rectStorage.Height());
		pGroupSchedule->MoveWindow(nScheduleLeft, rectSchedule.top, nScheduleWidth, rectSchedule.Height());
		pGroupEnd->MoveWindow(nEndLeft, rectEnd.top, nEndWidth, rectEnd.Height());

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

		CWnd* arrEndChild[] = { pEditEndValue, pTextEndAfter };
		for (int i = 0; i < 2; ++i)
		{
			CWnd* pChild = arrEndChild[i];
			if (pChild == nullptr || !pChild->GetSafeHwnd())
			{
				continue;
			}

			CRect rectChild;
			pChild->GetWindowRect(&rectChild);
			ScreenToClient(&rectChild);

			pChild->MoveWindow(rectChild.left + nEndOffsetX, rectChild.top, rectChild.Width(), rectChild.Height());
		}
	}
}

void CSetupItem::LoadFromTemplate(const CParameter::PARAM_TEMPLATE& paramTemplate)
{
	SetDlgItemText(IDC_EDIT_SETUPITEM_NAME, FindTemplateValue(paramTemplate, KEY_NAME, paramTemplate.strName));
	SetDlgItemText(IDC_EDIT_SETUPITEM_PATH_ORIGIN, FindTemplateValue(paramTemplate, KEY_ORIGIN_PATH));
	SetDlgItemText(IDC_EDIT_SETUPITEM_PATH_DEST, FindTemplateValue(paramTemplate, KEY_DEST_PATH));
	CheckDlgButton(IDC_CK_SETUPITEM_ENABLEMOVE,
		FindTemplateValue(paramTemplate, KEY_ENABLE_MOVE, _T("0")) == _T("1") ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CK_SETUPITEM_BOOTSTART,
		FindTemplateValue(paramTemplate, KEY_BOOT_START, _T("0")) == _T("1") ? BST_CHECKED : BST_UNCHECKED);

	const CString strLimitMode = FindTemplateValue(paramTemplate, KEY_LIMIT_MODE, LIMIT_MODE_STORAGE);
	CheckRadioButton(IDC_RADIO_SETUPITEM_LIMIT_STORAGE, IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE,
		strLimitMode == LIMIT_MODE_SCHEDULE ? IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE : IDC_RADIO_SETUPITEM_LIMIT_STORAGE);

	SetDlgItemText(IDC_EDIT_SETUPITEM_LIMIT_VALUE, FindTemplateValue(paramTemplate, KEY_LIMIT_VALUE));
	SetDlgItemText(IDC_EDIT_SETUPITEM_END_VALUE, FindTemplateValue(paramTemplate, KEY_END_VALUE));
	const CString strDriveName = FindTemplateValue(paramTemplate, KEY_DRIVE_NAME);

	const CString strScheduleDays = FindTemplateValue(paramTemplate, KEY_SCHEDULE_DAYS);
	CComboBox* pComboScheduleDays = (CComboBox*)GetDlgItem(IDC_COMBO_SETUPITEM_LIMIT_SCHEJULE_DAYS);
	if (pComboScheduleDays != nullptr && pComboScheduleDays->GetSafeHwnd())
	{
		pComboScheduleDays->SelectString(-1, strScheduleDays);
	}
	SetDlgItemText(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME, FindTemplateValue(paramTemplate, KEY_SCHEDULE_TIME));

	UpdateLimitControls();
	UpdateMoveControls();
}

void CSetupItem::SaveToTemplate(CParameter::PARAM_TEMPLATE& paramTemplate)
{
	CString strValue;

	GetDlgItemText(IDC_EDIT_SETUPITEM_NAME, strValue);
	strValue.Trim();
	paramTemplate.strName = strValue;
	AddTemplateValue(paramTemplate, KEY_NAME, strValue);

	GetDlgItemText(IDC_EDIT_SETUPITEM_PATH_ORIGIN, strValue);
	strValue.Trim();
	AddTemplateValue(paramTemplate, KEY_ORIGIN_PATH, strValue);

	GetDlgItemText(IDC_EDIT_SETUPITEM_PATH_DEST, strValue);
	strValue.Trim();
	if (IsDlgButtonChecked(IDC_CK_SETUPITEM_ENABLEMOVE) != BST_CHECKED)
	{
		strValue.Empty();
	}
	AddTemplateValue(paramTemplate, KEY_DEST_PATH, strValue);

	const BOOL bEnableMove = IsDlgButtonChecked(IDC_CK_SETUPITEM_ENABLEMOVE) == BST_CHECKED;
	strValue = bEnableMove ? _T("1") : _T("0");
	AddTemplateValue(paramTemplate, KEY_ENABLE_MOVE, strValue);

	strValue = IsDlgButtonChecked(IDC_CK_SETUPITEM_BOOTSTART) == BST_CHECKED ? _T("1") : _T("0");
	AddTemplateValue(paramTemplate, KEY_BOOT_START, strValue);

	const BOOL bScheduleMode = IsDlgButtonChecked(IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE) == BST_CHECKED;
	strValue = bScheduleMode ? LIMIT_MODE_SCHEDULE : LIMIT_MODE_STORAGE;
	AddTemplateValue(paramTemplate, KEY_LIMIT_MODE, strValue);

	GetDlgItemText(IDC_EDIT_SETUPITEM_LIMIT_VALUE, strValue);
	strValue.Trim();
	if (bScheduleMode)
	{
		strValue.Empty();
	}
	AddTemplateValue(paramTemplate, KEY_LIMIT_VALUE, strValue);

	GetDlgItemText(IDC_EDIT_SETUPITEM_END_VALUE, strValue);
	strValue.Trim();
	AddTemplateValue(paramTemplate, KEY_END_VALUE, strValue);

	strValue.Empty();
	AddTemplateValue(paramTemplate, KEY_DRIVE_NAME, strValue);

	strValue.Empty();
	if (bScheduleMode)
	{
		CComboBox* pComboScheduleDays = (CComboBox*)GetDlgItem(IDC_COMBO_SETUPITEM_LIMIT_SCHEJULE_DAYS);
		if (pComboScheduleDays != nullptr && pComboScheduleDays->GetSafeHwnd())
		{
			const int nCurSel = pComboScheduleDays->GetCurSel();
			if (nCurSel != CB_ERR)
			{
				pComboScheduleDays->GetLBText(nCurSel, strValue);
				strValue.Trim();
			}
		}
	}
	else
	{
		strValue.Empty();
	}
	AddTemplateValue(paramTemplate, KEY_SCHEDULE_DAYS, strValue);

	GetDlgItemText(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME, strValue);
	strValue.Trim();
	if (!bScheduleMode)
	{
		strValue.Empty();
	}
	AddTemplateValue(paramTemplate, KEY_SCHEDULE_TIME, strValue);
}

void CSetupItem::OnBnClickedRadioLimitStorage()
{
	UpdateLimitControls();
}

void CSetupItem::OnBnClickedRadioLimitSchedule()
{
	UpdateLimitControls();
}

void CSetupItem::OnBnClickedCheckEnableMove()
{
	UpdateMoveControls();
}

void CSetupItem::OnEnChangeEditLimitValue()
{
	NormalizeNumericEdit(IDC_EDIT_SETUPITEM_LIMIT_VALUE, 3);
}

void CSetupItem::OnEnChangeEditEndValue()
{
	NormalizeNumericEdit(IDC_EDIT_SETUPITEM_END_VALUE, 3);
}

void CSetupItem::OnEnChangeEditScheduleTime()
{
	NormalizeNumericEdit(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME, 4);
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

void CSetupItem::NormalizeNumericEdit(UINT nControlID, int nMaxLength)
{
	CEdit* pEdit = (CEdit*)GetDlgItem(nControlID);
	if (pEdit == nullptr || !pEdit->GetSafeHwnd())
	{
		return;
	}

	CString strValue;
	pEdit->GetWindowText(strValue);

	CString strFiltered;
	for (int i = 0; i < strValue.GetLength() && strFiltered.GetLength() < nMaxLength; ++i)
	{
		if (_istdigit(strValue[i]))
		{
			strFiltered += strValue[i];
		}
	}

	if (strFiltered != strValue)
	{
		pEdit->SetWindowText(strFiltered);
		pEdit->SetSel(strFiltered.GetLength(), strFiltered.GetLength());
	}
}

void CSetupItem::UpdateMoveControls()
{
	CWnd* pEditDest = GetDlgItem(IDC_EDIT_SETUPITEM_PATH_DEST);
	if (pEditDest != nullptr && pEditDest->GetSafeHwnd())
	{
		pEditDest->EnableWindow(IsDlgButtonChecked(IDC_CK_SETUPITEM_ENABLEMOVE) == BST_CHECKED);
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

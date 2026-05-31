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
	ON_EN_CHANGE(IDC_EDIT_SETUPITEM_PATH_ORIGIN, &CSetupItem::OnEnChangeEditOriginPath)
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
	UpdateDriveNameFromTargetPath();
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
	SetDlgItemText(IDC_EDIT_SETUPITEM_NAME, paramTemplate.strName);
	SetDlgItemText(IDC_EDIT_SETUPITEM_PATH_ORIGIN, paramTemplate.strOriginPath);
	SetDlgItemText(IDC_EDIT_SETUPITEM_PATH_DEST, paramTemplate.strDestPath);
	CheckDlgButton(IDC_CK_SETUPITEM_ENABLEMOVE,
		paramTemplate.bEnableMove ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CK_SETUPITEM_BOOTSTART,
		paramTemplate.bBootStart ? BST_CHECKED : BST_UNCHECKED);

	CheckRadioButton(IDC_RADIO_SETUPITEM_LIMIT_STORAGE, IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE,
		paramTemplate.strLimitMode == CParameter::TemplateKey::LIMIT_MODE_SCHEDULE ? IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE : IDC_RADIO_SETUPITEM_LIMIT_STORAGE);

	SetDlgItemText(IDC_EDIT_SETUPITEM_LIMIT_VALUE, paramTemplate.strLimitValue);
	SetDlgItemText(IDC_EDIT_SETUPITEM_END_VALUE, paramTemplate.strEndValue);
	UpdateDriveNameFromTargetPath();

	CComboBox* pComboScheduleDays = (CComboBox*)GetDlgItem(IDC_COMBO_SETUPITEM_LIMIT_SCHEJULE_DAYS);
	if (pComboScheduleDays != nullptr && pComboScheduleDays->GetSafeHwnd())
	{
		pComboScheduleDays->SelectString(-1, paramTemplate.strScheduleDays);
	}
	SetDlgItemText(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME, paramTemplate.strScheduleTime);

	UpdateLimitControls();
	UpdateMoveControls();
	UpdateDriveNameFromTargetPath();
}

void CSetupItem::SaveToTemplate(CParameter::PARAM_TEMPLATE& paramTemplate)
{
	CString strValue;

	GetDlgItemText(IDC_EDIT_SETUPITEM_NAME, strValue);
	strValue.Trim();
	paramTemplate.strName = strValue;

	GetDlgItemText(IDC_EDIT_SETUPITEM_PATH_ORIGIN, strValue);
	strValue.Trim();
	paramTemplate.strOriginPath = strValue;

	GetDlgItemText(IDC_EDIT_SETUPITEM_PATH_DEST, strValue);
	strValue.Trim();
	if (IsDlgButtonChecked(IDC_CK_SETUPITEM_ENABLEMOVE) != BST_CHECKED)
	{
		strValue.Empty();
	}
	paramTemplate.strDestPath = strValue;

	const BOOL bEnableMove = IsDlgButtonChecked(IDC_CK_SETUPITEM_ENABLEMOVE) == BST_CHECKED;
	paramTemplate.bEnableMove = bEnableMove;

	paramTemplate.bBootStart = IsDlgButtonChecked(IDC_CK_SETUPITEM_BOOTSTART) == BST_CHECKED;

	const BOOL bScheduleMode = IsDlgButtonChecked(IDC_RADIO_SETUPITEM_LIMIT_SCHEDULE) == BST_CHECKED;
	paramTemplate.strLimitMode = bScheduleMode ? CParameter::TemplateKey::LIMIT_MODE_SCHEDULE : CParameter::TemplateKey::LIMIT_MODE_STORAGE;

	GetDlgItemText(IDC_EDIT_SETUPITEM_LIMIT_VALUE, strValue);
	strValue.Trim();
	if (bScheduleMode)
	{
		strValue.Empty();
	}
	paramTemplate.strLimitValue = strValue;

	GetDlgItemText(IDC_EDIT_SETUPITEM_END_VALUE, strValue);
	strValue.Trim();
	paramTemplate.strEndValue = strValue;

	UpdateDriveNameFromTargetPath();
	paramTemplate.strDriveName = m_strDriveName;

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
	paramTemplate.strScheduleDays = strValue;

	GetDlgItemText(IDC_EDIT_SETUPITEM_LIMIT_SCHEJULE_TIME, strValue);
	strValue.Trim();
	if (!bScheduleMode)
	{
		strValue.Empty();
	}
	paramTemplate.strScheduleTime = strValue;
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

void CSetupItem::UpdateDriveNameFromTargetPath()
{
	CString strTargetPath;
	GetDlgItemText(IDC_EDIT_SETUPITEM_PATH_ORIGIN, strTargetPath);

	m_strDriveName = ParseDriveNameFromPath(strTargetPath);

	CString strDisplay;
	if (!m_strDriveName.IsEmpty())
	{
		strDisplay.Format(_T("%s:"), static_cast<LPCTSTR>(m_strDriveName));
	}
	else
	{
		strDisplay = _T("-");
	}

	CWnd* pTextDriveName = GetDlgItem(IDC_STATIC_SETUPITEM_DRIVENAME);
	if (pTextDriveName != nullptr && pTextDriveName->GetSafeHwnd())
	{
		pTextDriveName->SetWindowText(strDisplay);
	}
}

CString CSetupItem::ParseDriveNameFromPath(const CString& strPath) const
{
	CString strDriveName;
	CString strValue = strPath;
	strValue.Trim();

	if (strValue.GetLength() >= 2 && strValue[1] == _T(':') && _istalpha(strValue[0]))
	{
		strDriveName.Format(_T("%c"), static_cast<TCHAR>(_totupper(strValue[0])));
	}

	return strDriveName;
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

void CSetupItem::OnEnChangeEditOriginPath()
{
	UpdateDriveNameFromTargetPath();
}

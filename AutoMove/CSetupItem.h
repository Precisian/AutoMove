#pragma once
#include "afxdialogex.h"
#include "CParameter.h"


// CSetupItem 대화 상자

class CSetupItem : public CDialogEx
{
	DECLARE_DYNAMIC(CSetupItem)

public:
	CSetupItem(CWnd* pParent = nullptr,
		const std::vector<CString>& vecAvailableDriveNames = std::vector<CString>());   // 표준 생성자입니다.
	virtual ~CSetupItem() = default;

	void AlignControls();
	void LoadFromTemplate(const CParameter::PARAM_TEMPLATE& paramTemplate);
	void SaveToTemplate(CParameter::PARAM_TEMPLATE& paramTemplate);
	void SetBootStart(BOOL bBootStart);
	BOOL IsBootStart() const;

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SETUPITEM_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedRadioLimitStorage();
	afx_msg void OnBnClickedRadioLimitSchedule();
	afx_msg void OnBnClickedCheckEnableMove();
	afx_msg void OnBnClickedBtnSetupitemRemove();
	afx_msg void OnEnChangeEditLimitValue();
	afx_msg void OnEnChangeEditScheduleTime();

	DECLARE_MESSAGE_MAP()

private:
	void UpdateLimitControls();
	void UpdateMoveControls();
	void NormalizeNumericEdit(UINT nControlID, int nMaxLength);
	void LoadDriveNamesToControl();
	CWnd* FindGroupBox(LPCTSTR lpszText);
	CWnd* FindChildByText(LPCTSTR lpszText);

	std::vector<CString> m_vecAvailableDriveNames;
};

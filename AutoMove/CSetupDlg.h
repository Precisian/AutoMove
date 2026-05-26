#pragma once
#include <afxdialogex.h>
#include "resource.h"
#include "CParameter.h"
#include "CListScrollView.h"

class CSetupDlg : public CDialogEx
{
public:
	CSetupDlg(CWnd* pParent, CParameter* pRuntimeParam = nullptr);
	virtual ~CSetupDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SETUP_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedBtSystemSave();
	afx_msg void OnBnClickedBtSystemExit();

	CParameter m_param;

	CListScrollView* m_pScrollView;
	afx_msg void OnBnClickedBtnSystemAdditem();
	afx_msg void OnBnClickedCheckAutoStart();
	afx_msg void OnBnClickedBtnSetupTestStart();

private:
	void LoadParameterToControls();
	BOOL SaveControlsToParameter(CString& strErrorMessage);
	void BuildTemplateValidationErrors(const CParameter::PARAM_TEMPLATE& paramTemplate, std::vector<CString>& vecErrors) const;
	void AddTemplateItem(const CParameter::PARAM_TEMPLATE* pTemplate = nullptr);
	void SetAllTemplateBootStart(BOOL bBootStart);

	CParameter* m_pRuntimeParam;
};

#pragma once
#include <afxdialogex.h>
#include "resource.h"
#include "CParameter.h"
#include "CListScrollView.h"

class CSetupDlg : public CDialogEx
{
public:
	CSetupDlg(CWnd* pParent);
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

	CParameter m_pParam;
	CParameter m_pParam_backup;

	CListScrollView* m_pScrollView;
};

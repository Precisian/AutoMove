#pragma once
#include "afxdialogex.h"


// CSetupItem 대화 상자

class CSetupItem : public CDialogEx
{
	DECLARE_DYNAMIC(CSetupItem)

public:
	CSetupItem(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CSetupItem();

	void AlignControls();

	CString m_strName;
	CString m_strPath_Origin;
	CString m_strPath_Dest;
	bool m_bUseMove;

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SETUPITEM_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedRadioLimitStorage();
	afx_msg void OnBnClickedRadioLimitSchedule();
	afx_msg void OnBnClickedBtnSetupitemRemove();

	DECLARE_MESSAGE_MAP()

private:
	void UpdateLimitControls();
	CWnd* FindGroupBox(LPCTSTR lpszText);
	CWnd* FindChildByText(LPCTSTR lpszText);
};

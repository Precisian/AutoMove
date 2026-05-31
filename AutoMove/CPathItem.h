#pragma once
#include "afxdialogex.h"
#include "CParameter.h"

constexpr UINT WM_PATHITEM_STATE_CHANGED = WM_APP + 2;


// CPathItem 대화 상자

class CPathItem : public CDialogEx
{
	DECLARE_DYNAMIC(CPathItem)

public:
	CPathItem(CWnd* pParent = nullptr, CString strPathName = _T(""));   // 표준 생성자입니다.
	virtual ~CPathItem();
	void LoadFromTemplate(const CParameter::PARAM_TEMPLATE& paramTemplate);
	void SetPathName(LPCTSTR lpszPathName);
	void SetEventText(LPCTSTR lpszEventText);
	void SetWaitingEvent(BOOL bWaitingEvent, BOOL bNotifyStateChanged = TRUE);
	void SetWorkingMoveCopy(BOOL bWorkingMoveCopy, BOOL bNotifyStateChanged = TRUE);
	BOOL IsWaitingEvent() const;
	BOOL IsWorkingMoveCopy() const;
	void SetBlinkOn(BOOL bBlinkOn);
	void SetEventRunningText(BOOL bRunning);
	void RefreshActControl();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PATHITEM_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.
	virtual void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnBnClickedPathitemStart();
	afx_msg void OnBnClickedPathitemStop();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	CString m_strPathName;

private:
	COLORREF GetActColor() const;
	void RefreshEventText();
	void UpdateButtons();
	void NotifyStateChanged();

	CString m_strEventText;
	BOOL m_bWaitingEvent;
	BOOL m_bWorkingMoveCopy;
	BOOL m_bBlinkOn;
	BOOL m_bEventRunningText;
};

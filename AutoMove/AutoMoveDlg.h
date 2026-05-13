// AutoMoveDlg.h: 헤더 파일
//

#pragma once
#include "CListScrollView.h"
#include "CSetupDlg.h"
#include "CParameter.h"

#define WM_TRAY_ICON (WM_USER + 1)

// CAutoMoveDlg 대화 상자
class CAutoMoveDlg : public CDialogEx
{
// 생성입니다.
public:
	CAutoMoveDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.
	NOTIFYICONDATA m_nId; // 트레이 아이콘 구조체

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUTOMOVE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedMainExit();
	DECLARE_MESSAGE_MAP()
public:
	CListScrollView* m_pScrollView{};
	CParameter m_pParam;
	BOOL m_bPathItemBlinkOn;
	BOOL m_bPathItemBlinkTimerActive;

	void AlignControls();
	void SetTrayIcon();
	void ReloadPathItems();
	void NotifyPathItemBlink();
	void UpdatePathItemBlinkTimer();
	BOOL HasBlinkingPathItem() const;

	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnBnClickedBtSystemOpen();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnPathItemStateChanged(WPARAM wParam, LPARAM lParam);
	
	
	// 트레이 아이콘 메시지 처리
	afx_msg LRESULT OnTrayIcon(WPARAM wParam, LPARAM lParam); 
	afx_msg void OnTrayOpen();
	afx_msg void OnTrayExit();
};

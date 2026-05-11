// AutoMoveDlg.h: 헤더 파일
//

#pragma once
#include "CListScrollView.h"
#include "CSystemDlg.h"

// CAutoMoveDlg 대화 상자
class CAutoMoveDlg : public CDialogEx
{
// 생성입니다.
public:
	CAutoMoveDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUTOMOVE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

// 구현입니다.
protected:
	HICON m_hIcon;

	CListScrollView* m_pScrollView{};
	CSystemDlg* m_pSystemDlg{};


	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedMainExit();
	DECLARE_MESSAGE_MAP()
public:
	enum DIR_TYPE {
		DIR_LEFT,
		DIR_UP,
		DIR_RIGHT,
		DIR_DOWN,
	};

	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

	void AlignControls();
	afx_msg void OnBnClickedBtSystemOpen();
};

#pragma once
#include "afxdialogex.h"


// CPathItem 대화 상자

class CPathItem : public CDialogEx
{
	DECLARE_DYNAMIC(CPathItem)

public:
	CPathItem(CWnd* pParent = nullptr, CString strPathName = _T(""));   // 표준 생성자입니다.
	virtual ~CPathItem();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PATHITEM_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.
	virtual void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
public:
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

	CString m_strPathName;
};
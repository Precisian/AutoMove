// CPathItem.cpp: 구현 파일
//

#include "pch.h"
#include "resource.h"
#include "afxdialogex.h"
#include "CPathItem.h"


// CPathItem 대화 상자

IMPLEMENT_DYNAMIC(CPathItem, CDialogEx)

CPathItem::CPathItem(CWnd* pParent, CString strPathName)
	: CDialogEx(IDD_PATHITEM_DIALOG, pParent)
{
	m_strPathName = strPathName;
}

CPathItem::~CPathItem()
{
}

void CPathItem::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CPathItem, CDialogEx)
	ON_WM_MOUSEWHEEL()
    ON_WM_SIZE()
END_MESSAGE_MAP()


BOOL CPathItem::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// 현재 아이템에서 마우스 휠 이벤트 부모에게 전달
	return GetParent()->SendMessage(WM_MOUSEWHEEL, MAKEWPARAM(nFlags, zDelta), MAKELPARAM(pt.x, pt.y));
}

void CPathItem::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);

    if (cx <= 0) return;

    CWnd* pBtn1 = GetDlgItem(IDC_BT_PATHITEM_STOP);
    CWnd* pBtn2 = GetDlgItem(IDC_BT_PATHITEM_START);
    CWnd* pEdit = GetDlgItem(IDC_STATIC_PATHITEM_STATUS);

    if (pBtn1 && pBtn2 && pEdit && pBtn1->GetSafeHwnd()) {
        int rightMargin = 20; // 가장 우측 끝에서 띄울 간격
        int elementGap = 10;  // 요소들 사이의 간격
        int btnWidth = 45;
        int btnHeight = 30;

        // 1. 가장 우측 버튼 (Stop 버튼)
        // 전체 폭(cx) - 우측 여백(20) - 버튼 너비
        int b1X = cx - rightMargin - btnWidth;
        int b1Y = (cy - btnHeight) / 2;
        pBtn1->MoveWindow(b1X, b1Y, btnWidth, btnHeight);

        // 2. 우측에서 두 번째 버튼 (Start 버튼)
        // Stop 버튼의 X좌표(b1X) - 요소 간격(10) - 버튼 너비
        int b2X = b1X - elementGap - btnWidth;
        int b2Y = b1Y; // 동일 선상
        pBtn2->MoveWindow(b2X, b2Y, btnWidth, btnHeight);

    }
}
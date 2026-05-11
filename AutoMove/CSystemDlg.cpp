#include "pch.h"
#include "CSystemDlg.h"

// 3. 부모 지정 생성자 (일반 DoModal 용)
CSystemDlg::CSystemDlg(CWnd* pParent)
    : CDialogEx(IDD_SYSTEM_DIALOG, pParent)
{
}

CSystemDlg::~CSystemDlg()
{
}

void CSystemDlg::DoDataExchange(CDataExchange* pDX)
{
    // 주의: 클래스 이름을 본인 클래스인 CSystemDlg로 호출해야 합니다.
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSystemDlg, CDialogEx)
END_MESSAGE_MAP()
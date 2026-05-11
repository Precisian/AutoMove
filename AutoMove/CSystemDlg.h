#pragma once
#include <afxdialogex.h>
#include "resource.h"

class CSystemDlg : public CDialogEx
{

public:
    CSystemDlg(CWnd* pParent);

    virtual ~CSystemDlg();

    // 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_SYSTEM_DIALOG }; // IDD_SYSTEM_DIALOG로 통일
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

    DECLARE_MESSAGE_MAP()
};
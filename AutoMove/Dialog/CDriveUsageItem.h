#pragma once
#include "afxdialogex.h"
#include "../Resource.h"
#include "../DriveInfo.h"

class CDriveUsageItem : public CDialogEx
{
	DECLARE_DYNAMIC(CDriveUsageItem)

public:
	CDriveUsageItem(CWnd* pParent = nullptr);
	virtual ~CDriveUsageItem();

	void SetDriveInfo(const DRIVE_INFO& driveInfo);
	CString GetDriveName() const;

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DRIVEUSAGEITEM_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

private:
	void AlignControls();

	CString m_strDriveName;
	int m_nUsagePercent;
};

// AutoMoveDlg.h: 헤더 파일
//

#pragma once
#include "CListScrollView.h"
#include "CSetupDlg.h"
#include "CParameter.h"
#include "DriveInfo.h"
#include "Dialog/CDriveUsageItem.h"
#include "Manager/CDriveTaskWorker.h"
#include <vector>

#define WM_TRAY_ICON (WM_USER + 1)
#define WM_DRIVE_USAGE_UPDATED (WM_USER + 2)

// CAutoMoveDlg 대화 상자
class CAutoMoveDlg : public CDialogEx
{
// 생성입니다.
public:
	CAutoMoveDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.
	virtual ~CAutoMoveDlg();
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
	std::vector<CString> m_vecAvailableDriveNames;
	CWinThread* m_pDriveUsageThread;
	HANDLE m_hDriveUsageStopEvent;
	std::vector<DRIVE_INFO> m_vecDriveInfos;
	std::vector<CDriveUsageItem*> m_vecDriveUsageItems;
	CDriveTaskWorker m_driveTaskWorker;

	void AlignControls();
	void SetTrayIcon();
	void ReloadPathItems();
	void NotifyPathItemBlink();
	void UpdatePathItemBlinkTimer();
	BOOL HasBlinkingPathItem() const;
	void LoadAvailableDriveNames();
	void CreateDriveUsageControls();
	void DestroyDriveUsageControls();
	void UpdateDriveUsageControls(const std::vector<DRIVE_INFO>& vecDriveInfos);
	int GetDriveUsageHeightDelta() const;
	int GetFixedWindowHeight() const;
	void UpdateFixedWindowSize();
	void StartDriveUsageThread();
	void StopDriveUsageThread();
	static UINT DriveUsageThreadProc(LPVOID pParam);
	BOOL EnsureDriveTaskWorkerStarted();
	CParameter::PARAM_TEMPLATE* FindTemplateByName(LPCTSTR lpszTemplateName);
	const CParameter::PARAM_TEMPLATE* FindTemplateByName(LPCTSTR lpszTemplateName) const;
	CPathItem* FindPathItemByHwnd(HWND hPathItemWnd) const;
	CPathItem* FindPathItemByTemplateName(LPCTSTR lpszTemplateName) const;
	BOOL EnqueueDriveTask(CParameter::PARAM_TEMPLATE& paramTemplate, CPathItem* pPathItem);
	void MarkScheduleTaskRunDate(CParameter::PARAM_TEMPLATE& paramTemplate);
	DRIVE_TASK BuildDriveTask(const CParameter::PARAM_TEMPLATE& paramTemplate, CPathItem* pPathItem) const;
	void ResetPathItemTaskState();
	int EnqueueTriggeredDriveTasks(const std::vector<DRIVE_INFO>& vecDriveInfos);
	BOOL ShouldTriggerDriveTask(const CParameter::PARAM_TEMPLATE& paramTemplate, const std::vector<DRIVE_INFO>& vecDriveInfos) const;
	BOOL ShouldTriggerScheduleTask(const CParameter::PARAM_TEMPLATE& paramTemplate, const SYSTEMTIME& now) const;
	BOOL IsScheduleDayMatched(const CString& strScheduleDays, WORD wDayOfWeek) const;
	CString GetScheduleRunDate(const SYSTEMTIME& time) const;

	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnBnClickedBtSystemOpen();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnPathItemStateChanged(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDriveUsageUpdated(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDriveTaskStarted(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDriveTaskFinished(WPARAM wParam, LPARAM lParam);
	
	
	// 트레이 아이콘 메시지 처리
	afx_msg LRESULT OnTrayIcon(WPARAM wParam, LPARAM lParam); 
	afx_msg void OnTrayOpen();
	afx_msg void OnTrayExit();
	afx_msg void OnBnClickedBtnMainAllStart();
	afx_msg void OnBnClickedBtnMainAllStop();
};

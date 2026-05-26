#pragma once

#include <afxmt.h>
#include <deque>
#include <vector>

constexpr UINT WM_DRIVE_TASK_STARTED = WM_APP + 20;
constexpr UINT WM_DRIVE_TASK_FINISHED = WM_APP + 21;

enum class DRIVE_TASK_TYPE
{
	RemoveFiles,
	MoveFiles
};

enum class DRIVE_TASK_RESULT
{
	Completed,
	Canceled,
	Failed
};

struct DRIVE_TASK
{
	DRIVE_TASK_TYPE eType = DRIVE_TASK_TYPE::RemoveFiles;
	HWND hPathItemWnd = nullptr;
	CString strTemplateName;
	CString strOriginPath;
	CString strDestPath;
	CString strDriveName;
	int nEndUsagePercent = 0;
};

struct DRIVE_TASK_NOTIFY
{
	HWND hPathItemWnd = nullptr;
	CString strTemplateName;
	DRIVE_TASK_RESULT eResult = DRIVE_TASK_RESULT::Completed;
	CString strMessage;
};

class CDriveTaskWorker
{
public:
	CDriveTaskWorker() = default;
	~CDriveTaskWorker();

	CDriveTaskWorker(const CDriveTaskWorker&) = delete;
	CDriveTaskWorker& operator=(const CDriveTaskWorker&) = delete;

	BOOL Start(HWND hOwnerWnd);
	void Stop();
	BOOL IsRunning() const;

	BOOL Enqueue(const DRIVE_TASK& task);
	BOOL Cancel(HWND hPathItemWnd);
	BOOL Cancel(LPCTSTR lpszTemplateName);
	BOOL IsQueued(HWND hPathItemWnd) const;
	BOOL IsWorking(HWND hPathItemWnd) const;

private:
	struct TASK_MATCH_KEY
	{
		HWND hPathItemWnd = nullptr;
		CString strTemplateName;
	};

	static UINT ThreadProc(LPVOID pParam);
	UINT Run();

	BOOL PopTask(DRIVE_TASK& task);
	void ResetQueueEventIfEmpty();
	void ClearQueue();
	BOOL CancelTask(const TASK_MATCH_KEY& key);
	TASK_MATCH_KEY MakeTaskMatchKey(HWND hPathItemWnd) const;
	TASK_MATCH_KEY MakeTaskMatchKey(LPCTSTR lpszTemplateName) const;
	BOOL MatchesTask(const DRIVE_TASK& task, const TASK_MATCH_KEY& key) const;
	BOOL MatchesWorkingTask(const TASK_MATCH_KEY& key) const;
	BOOL HasQueuedTask(const TASK_MATCH_KEY& key) const;
	void NotifyStarted(const DRIVE_TASK& task) const;
	void NotifyFinished(const DRIVE_TASK& task, DRIVE_TASK_RESULT eResult, LPCTSTR lpszMessage) const;
	void NotifyCanceledTasks(const std::vector<DRIVE_TASK>& vecCanceledTasks) const;
	DRIVE_TASK_RESULT ExecuteTask(const DRIVE_TASK& task, CString& strMessage);
	BOOL ShouldCancelCurrent() const;
	BOOL HasReachedEndUsage(const DRIVE_TASK& task) const;

	HWND m_hOwnerWnd = nullptr;
	CWinThread* m_pThread = nullptr;
	HANDLE m_hStopEvent = nullptr;
	HANDLE m_hQueueEvent = nullptr;

	mutable CCriticalSection m_csQueue;
	std::deque<DRIVE_TASK> m_queue;

	mutable CCriticalSection m_csState;
	HWND m_hWorkingPathItemWnd = nullptr;
	CString m_strWorkingTemplateName;
	BOOL m_bCancelCurrent = FALSE;
};

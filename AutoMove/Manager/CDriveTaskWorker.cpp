#include "pch.h"
#include "CDriveTaskWorker.h"
#include "CDriveManager.h"
#include <vector>

namespace
{
	constexpr DWORD TASK_WAIT_TIMEOUT = 250;

	BOOL IsStopRequested(HANDLE hStopEvent)
	{
		return hStopEvent != nullptr
			&& WaitForSingleObject(hStopEvent, 0) == WAIT_OBJECT_0;
	}
}

CDriveTaskWorker::~CDriveTaskWorker()
{
	Stop();
}

BOOL CDriveTaskWorker::Start(HWND hOwnerWnd)
{
	if (m_pThread != nullptr)
	{
		return TRUE;
	}

	m_hOwnerWnd = hOwnerWnd;
	m_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	m_hQueueEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (m_hStopEvent == nullptr || m_hQueueEvent == nullptr)
	{
		Stop();
		return FALSE;
	}

	m_pThread = AfxBeginThread(ThreadProc, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (m_pThread == nullptr)
	{
		Stop();
		return FALSE;
	}

	m_pThread->m_bAutoDelete = FALSE;
	m_pThread->ResumeThread();
	return TRUE;
}

void CDriveTaskWorker::Stop()
{
	if (m_hStopEvent != nullptr)
	{
		SetEvent(m_hStopEvent);
	}

	if (m_hQueueEvent != nullptr)
	{
		SetEvent(m_hQueueEvent);
	}

	if (m_pThread != nullptr)
	{
		WaitForSingleObject(m_pThread->m_hThread, INFINITE);
		delete m_pThread;
		m_pThread = nullptr;
	}

	ClearQueue();

	if (m_hQueueEvent != nullptr)
	{
		CloseHandle(m_hQueueEvent);
		m_hQueueEvent = nullptr;
	}

	if (m_hStopEvent != nullptr)
	{
		CloseHandle(m_hStopEvent);
		m_hStopEvent = nullptr;
	}

	CSingleLock stateLock(&m_csState, TRUE);
	m_hWorkingPathItemWnd = nullptr;
	m_strWorkingTemplateName.Empty();
	m_bCancelCurrent = FALSE;
	m_hOwnerWnd = nullptr;
}

BOOL CDriveTaskWorker::IsRunning() const
{
	return m_pThread != nullptr;
}

BOOL CDriveTaskWorker::Enqueue(const DRIVE_TASK& task)
{
	if (m_hQueueEvent == nullptr || m_hStopEvent == nullptr)
	{
		return FALSE;
	}

	if (IsStopRequested(m_hStopEvent))
	{
		return FALSE;
	}

	{
		CSingleLock stateLock(&m_csState, TRUE);
		if (MatchesWorkingTask(MakeTaskMatchKey(task.hPathItemWnd))
			|| MatchesWorkingTask(MakeTaskMatchKey(task.strTemplateName)))
		{
			return FALSE;
		}
	}

	{
		CSingleLock queueLock(&m_csQueue, TRUE);
		for (const DRIVE_TASK& queuedTask : m_queue)
		{
			if (MatchesTask(queuedTask, MakeTaskMatchKey(task.hPathItemWnd))
				|| MatchesTask(queuedTask, MakeTaskMatchKey(task.strTemplateName)))
			{
				return FALSE;
			}
		}
		m_queue.push_back(task);
	}

	SetEvent(m_hQueueEvent);
	return TRUE;
}

BOOL CDriveTaskWorker::Cancel(HWND hPathItemWnd)
{
	return CancelTask(MakeTaskMatchKey(hPathItemWnd));
}

BOOL CDriveTaskWorker::Cancel(LPCTSTR lpszTemplateName)
{
	return CancelTask(MakeTaskMatchKey(lpszTemplateName));
}

BOOL CDriveTaskWorker::CancelTask(const TASK_MATCH_KEY& key)
{
	BOOL bCanceled = FALSE;
	std::vector<DRIVE_TASK> vecCanceledTasks;

	{
		CSingleLock queueLock(&m_csQueue, TRUE);
		for (auto iter = m_queue.begin(); iter != m_queue.end();)
		{
			if (MatchesTask(*iter, key))
			{
				vecCanceledTasks.push_back(*iter);
				iter = m_queue.erase(iter);
				bCanceled = TRUE;
			}
			else
			{
				++iter;
			}
		}
	}

	ResetQueueEventIfEmpty();
	NotifyCanceledTasks(vecCanceledTasks);

	{
		CSingleLock stateLock(&m_csState, TRUE);
		if (MatchesWorkingTask(key))
		{
			m_bCancelCurrent = TRUE;
			bCanceled = TRUE;
		}
	}

	return bCanceled;
}
BOOL CDriveTaskWorker::IsQueued(HWND hPathItemWnd) const
{
	CSingleLock queueLock(&m_csQueue, TRUE);
	return HasQueuedTask(MakeTaskMatchKey(hPathItemWnd));
}

BOOL CDriveTaskWorker::IsWorking(HWND hPathItemWnd) const
{
	CSingleLock stateLock(&m_csState, TRUE);
	return m_hWorkingPathItemWnd == hPathItemWnd;
}

UINT CDriveTaskWorker::ThreadProc(LPVOID pParam)
{
	CDriveTaskWorker* pWorker = reinterpret_cast<CDriveTaskWorker*>(pParam);
	if (pWorker == nullptr)
	{
		return 0;
	}

	return pWorker->Run();
}

UINT CDriveTaskWorker::Run()
{
	HANDLE arrEvents[] = { m_hStopEvent, m_hQueueEvent };

	while (true)
	{
		const DWORD dwWait = WaitForMultipleObjects(2, arrEvents, FALSE, INFINITE);
		if (dwWait == WAIT_OBJECT_0)
		{
			break;
		}

		if (dwWait != WAIT_OBJECT_0 + 1)
		{
			continue;
		}

		DRIVE_TASK task;
		while (PopTask(task))
		{
			if (IsStopRequested(m_hStopEvent))
			{
				return 0;
			}

			{
				CSingleLock stateLock(&m_csState, TRUE);
				m_hWorkingPathItemWnd = task.hPathItemWnd;
				m_strWorkingTemplateName = task.strTemplateName;
				m_bCancelCurrent = FALSE;
			}

			NotifyStarted(task);

			CString strMessage;
			const DRIVE_TASK_RESULT eResult = ExecuteTask(task, strMessage);
			NotifyFinished(task, eResult, strMessage);

			{
				CSingleLock stateLock(&m_csState, TRUE);
				m_hWorkingPathItemWnd = nullptr;
				m_strWorkingTemplateName.Empty();
				m_bCancelCurrent = FALSE;
			}
		}
	}

	return 0;
}

BOOL CDriveTaskWorker::PopTask(DRIVE_TASK& task)
{
	CSingleLock queueLock(&m_csQueue, TRUE);
	if (m_queue.empty())
	{
		if (m_hQueueEvent != nullptr)
		{
			ResetEvent(m_hQueueEvent);
		}
		return FALSE;
	}

	task = m_queue.front();
	m_queue.pop_front();

	if (m_queue.empty() && m_hQueueEvent != nullptr)
	{
		ResetEvent(m_hQueueEvent);
	}

	return TRUE;
}

void CDriveTaskWorker::ResetQueueEventIfEmpty()
{
	CSingleLock queueLock(&m_csQueue, TRUE);
	if (m_queue.empty() && m_hQueueEvent != nullptr)
	{
		ResetEvent(m_hQueueEvent);
	}
}

void CDriveTaskWorker::ClearQueue()
{
	CSingleLock queueLock(&m_csQueue, TRUE);
	m_queue.clear();
	if (m_hQueueEvent != nullptr)
	{
		ResetEvent(m_hQueueEvent);
	}
}

void CDriveTaskWorker::NotifyStarted(const DRIVE_TASK& task) const
{
	if (m_hOwnerWnd == nullptr)
	{
		return;
	}

	DRIVE_TASK_NOTIFY* pNotify = new DRIVE_TASK_NOTIFY;
	pNotify->hPathItemWnd = task.hPathItemWnd;
	pNotify->strTemplateName = task.strTemplateName;

	if (!PostMessage(m_hOwnerWnd, WM_DRIVE_TASK_STARTED, 0, reinterpret_cast<LPARAM>(pNotify)))
	{
		delete pNotify;
	}
}

void CDriveTaskWorker::NotifyFinished(const DRIVE_TASK& task, DRIVE_TASK_RESULT eResult, LPCTSTR lpszMessage) const
{
	if (m_hOwnerWnd == nullptr)
	{
		return;
	}

	DRIVE_TASK_NOTIFY* pNotify = new DRIVE_TASK_NOTIFY;
	pNotify->hPathItemWnd = task.hPathItemWnd;
	pNotify->strTemplateName = task.strTemplateName;
	pNotify->eResult = eResult;
	pNotify->strMessage = lpszMessage;

	if (!PostMessage(m_hOwnerWnd, WM_DRIVE_TASK_FINISHED, 0, reinterpret_cast<LPARAM>(pNotify)))
	{
		delete pNotify;
	}
}

void CDriveTaskWorker::NotifyCanceledTasks(const std::vector<DRIVE_TASK>& vecCanceledTasks) const
{
	for (int i = 0; i < static_cast<int>(vecCanceledTasks.size()); ++i)
	{
		NotifyFinished(vecCanceledTasks[i], DRIVE_TASK_RESULT::Canceled, _T("작업이 취소되었습니다."));
	}
}

DRIVE_TASK_RESULT CDriveTaskWorker::ExecuteTask(const DRIVE_TASK& task, CString& strMessage)
{
	CString strOriginPath = task.strOriginPath;
	strOriginPath.Trim();
	if (strOriginPath.IsEmpty())
	{
		strMessage = _T("대상 경로가 비어 있습니다.");
		return DRIVE_TASK_RESULT::Failed;
	}

	CDriveFileManager driveFileManager;
	if (HasReachedEndUsage(task))
	{
		strMessage = _T("이미 종료 사용률 조건에 도달했습니다.");
		return DRIVE_TASK_RESULT::Completed;
	}

	CString strDestPath = task.strDestPath;
	strDestPath.Trim();
	if (task.eType == DRIVE_TASK_TYPE::MoveFiles && strDestPath.IsEmpty())
	{
		strMessage = _T("이동 경로가 비어 있습니다.");
		return DRIVE_TASK_RESULT::Failed;
	}

	const std::vector<CString> vecItems = task.eType == DRIVE_TASK_TYPE::MoveFiles
		? driveFileManager.FindMoveItems(strOriginPath)
		: driveFileManager.FindFiles(strOriginPath);
	for (int i = 0; i < static_cast<int>(vecItems.size()); ++i)
	{
		if (IsStopRequested(m_hStopEvent) || ShouldCancelCurrent())
		{
			strMessage = _T("작업이 취소되었습니다.");
			return DRIVE_TASK_RESULT::Canceled;
		}

		if (HasReachedEndUsage(task))
		{
			strMessage = _T("종료 사용률 조건에 도달했습니다.");
			return DRIVE_TASK_RESULT::Completed;
		}

		if (task.eType == DRIVE_TASK_TYPE::MoveFiles)
		{
			driveFileManager.MovePath(vecItems[i], strDestPath);
		}
		else
		{
			driveFileManager.RemovePath(vecItems[i]);
		}

		if (((i + 1) % 8) == 0)
		{
			if (WaitForSingleObject(m_hStopEvent, TASK_WAIT_TIMEOUT) == WAIT_OBJECT_0)
			{
				strMessage = _T("작업이 취소되었습니다.");
				return DRIVE_TASK_RESULT::Canceled;
			}
		}
	}

	strMessage = _T("작업이 완료되었습니다.");
	return DRIVE_TASK_RESULT::Completed;
}

BOOL CDriveTaskWorker::ShouldCancelCurrent() const
{
	CSingleLock stateLock(&m_csState, TRUE);
	return m_bCancelCurrent;
}

BOOL CDriveTaskWorker::HasReachedEndUsage(const DRIVE_TASK& task) const
{
	if (task.nEndUsagePercent <= 0 || task.strDriveName.IsEmpty())
	{
		return FALSE;
	}

	const int nUsagePercent = CDriveManager::GetDriveUsagePercent(task.strDriveName);
	return nUsagePercent >= 0 && nUsagePercent <= task.nEndUsagePercent;
}

CDriveTaskWorker::TASK_MATCH_KEY CDriveTaskWorker::MakeTaskMatchKey(HWND hPathItemWnd) const
{
	TASK_MATCH_KEY key;
	key.hPathItemWnd = hPathItemWnd;
	return key;
}

CDriveTaskWorker::TASK_MATCH_KEY CDriveTaskWorker::MakeTaskMatchKey(LPCTSTR lpszTemplateName) const
{
	TASK_MATCH_KEY key;
	if (lpszTemplateName != nullptr)
	{
		key.strTemplateName = lpszTemplateName;
		key.strTemplateName.Trim();
	}
	return key;
}

BOOL CDriveTaskWorker::MatchesTask(const DRIVE_TASK& task, const TASK_MATCH_KEY& key) const
{
	if (key.hPathItemWnd != nullptr)
	{
		return task.hPathItemWnd == key.hPathItemWnd;
	}

	return !key.strTemplateName.IsEmpty()
		&& task.strTemplateName.CompareNoCase(key.strTemplateName) == 0;
}

BOOL CDriveTaskWorker::MatchesWorkingTask(const TASK_MATCH_KEY& key) const
{
	if (key.hPathItemWnd != nullptr)
	{
		return m_hWorkingPathItemWnd == key.hPathItemWnd;
	}

	return !key.strTemplateName.IsEmpty()
		&& !m_strWorkingTemplateName.IsEmpty()
		&& m_strWorkingTemplateName.CompareNoCase(key.strTemplateName) == 0;
}

BOOL CDriveTaskWorker::HasQueuedTask(const TASK_MATCH_KEY& key) const
{
	for (const DRIVE_TASK& task : m_queue)
	{
		if (MatchesTask(task, key))
		{
			return TRUE;
		}
	}

	return FALSE;
}

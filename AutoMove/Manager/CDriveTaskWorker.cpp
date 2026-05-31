#include "pch.h"
#include "CDriveTaskWorker.h"
#include "CDriveManager.h"
#include "CLogManager.h"
#include "FileSystemUtil.h"
#include <vector>

namespace
{
	constexpr DWORD TASK_WAIT_TIMEOUT = 250;
	constexpr DWORD TASK_STATUS_MINIMUM_DISPLAY_INTERVAL = 500;

	BOOL IsStopRequested(HANDLE hStopEvent)
	{
		return hStopEvent != nullptr
			&& WaitForSingleObject(hStopEvent, 0) == WAIT_OBJECT_0;
	}

	LPCTSTR GetTaskTypeName(DRIVE_TASK_TYPE eType)
	{
		return eType == DRIVE_TASK_TYPE::MoveFiles ? _T("move") : _T("delete");
	}

	LPCTSTR GetTaskResultName(DRIVE_TASK_RESULT eResult)
	{
		switch (eResult)
		{
		case DRIVE_TASK_RESULT::Completed:
			return _T("completed");
		case DRIVE_TASK_RESULT::Canceled:
			return _T("canceled");
		case DRIVE_TASK_RESULT::Failed:
			return _T("failed");
		default:
			return _T("unknown");
		}
	}

	void LogTaskStarted(const DRIVE_TASK& task)
	{
		CString strMessage;
		strMessage.Format(_T("Task started. template=\"%s\", type=%s, origin=\"%s\", destination=\"%s\""),
			static_cast<LPCTSTR>(task.strTemplateName),
			GetTaskTypeName(task.eType),
			static_cast<LPCTSTR>(task.strOriginPath),
			static_cast<LPCTSTR>(task.strDestPath));
		CLogManager::Write(CLogManager::LOG_OPERATION, strMessage);
	}

	void LogTaskFinished(const DRIVE_TASK& task, DRIVE_TASK_RESULT eResult, const CString& strDetail)
	{
		CString strMessage;
		strMessage.Format(_T("Task finished. template=\"%s\", type=%s, result=%s, detail=\"%s\""),
			static_cast<LPCTSTR>(task.strTemplateName),
			GetTaskTypeName(task.eType),
			GetTaskResultName(eResult),
			static_cast<LPCTSTR>(strDetail));
		CLogManager::Write(CLogManager::LOG_OPERATION, strMessage);
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

	if (IsWorking(task.strTemplateName))
	{
		return FALSE;
	}

	{
		CSingleLock queueLock(&m_csQueue, TRUE);
		if (HasQueuedTask(task.strTemplateName))
		{
			return FALSE;
		}
		m_queue.push_back(task);
	}

	SetEvent(m_hQueueEvent);
	return TRUE;
}

BOOL CDriveTaskWorker::Cancel(LPCTSTR lpszTemplateName)
{
	return CancelTask(lpszTemplateName);
}

int CDriveTaskWorker::ClearPendingTasks()
{
	int nTaskCount = 0;
	{
		CSingleLock queueLock(&m_csQueue, TRUE);
		nTaskCount = static_cast<int>(m_queue.size());
		m_queue.clear();
		if (m_hQueueEvent != nullptr)
		{
			ResetEvent(m_hQueueEvent);
		}
	}

	CString strMessage;
	strMessage.Format(_T("Pending task queue cleared. count=%d"), nTaskCount);
	CLogManager::Write(CLogManager::LOG_OPERATION, strMessage);
	return nTaskCount;
}

BOOL CDriveTaskWorker::CancelTask(LPCTSTR lpszTemplateName)
{
	BOOL bCanceled = FALSE;
	std::vector<DRIVE_TASK> vecCanceledTasks;

	{
		CSingleLock queueLock(&m_csQueue, TRUE);
		for (auto iter = m_queue.begin(); iter != m_queue.end();)
		{
			if (MatchesTask(*iter, lpszTemplateName))
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

	if (IsWorking(lpszTemplateName))
	{
		CSingleLock stateLock(&m_csState, TRUE);
		m_bCancelCurrent = TRUE;
		bCanceled = TRUE;
	}

	return bCanceled;
}
BOOL CDriveTaskWorker::IsQueued(LPCTSTR lpszTemplateName) const
{
	CSingleLock queueLock(&m_csQueue, TRUE);
	return HasQueuedTask(lpszTemplateName);
}

BOOL CDriveTaskWorker::IsWorking(LPCTSTR lpszTemplateName) const
{
	CString strTemplateName;
	if (lpszTemplateName != nullptr)
	{
		strTemplateName = lpszTemplateName;
		strTemplateName.Trim();
	}

	CSingleLock stateLock(&m_csState, TRUE);
	return !strTemplateName.IsEmpty()
		&& !m_strWorkingTemplateName.IsEmpty()
		&& m_strWorkingTemplateName.CompareNoCase(strTemplateName) == 0;
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
				m_strWorkingTemplateName = task.strTemplateName;
				m_bCancelCurrent = FALSE;
			}

			LogTaskStarted(task);
			NotifyStarted(task);
			const ULONGLONG ullStartedTick = GetTickCount64();

			CString strMessage;
			const DRIVE_TASK_RESULT eResult = ExecuteTask(task, strMessage);
			LogTaskFinished(task, eResult, strMessage);

			const ULONGLONG ullElapsed = GetTickCount64() - ullStartedTick;
			if (ullElapsed < TASK_STATUS_MINIMUM_DISPLAY_INTERVAL)
			{
				Sleep(static_cast<DWORD>(TASK_STATUS_MINIMUM_DISPLAY_INTERVAL - ullElapsed));
			}
			NotifyFinished(task, eResult, strMessage);

			{
				CSingleLock stateLock(&m_csState, TRUE);
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

	if (!AutoMoveFileSystem::IsSafeWorkRoot(strOriginPath))
	{
		strMessage = _T("대상 경로가 안전한 작업 폴더가 아닙니다.");
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

	if (task.eType == DRIVE_TASK_TYPE::MoveFiles
		&& (AutoMoveFileSystem::IsSameOrChildPath(strOriginPath, strDestPath)
			|| AutoMoveFileSystem::IsSameOrChildPath(strDestPath, strOriginPath)))
	{
		strMessage = _T("이동 경로가 대상 경로와 같거나 서로의 하위 폴더입니다.");
		return DRIVE_TASK_RESULT::Failed;
	}

	DRIVE_TASK_RESULT eInterruptResult = DRIVE_TASK_RESULT::Completed;
	const auto continueWork = [this, &task, &strMessage, &eInterruptResult]()
	{
		if (IsStopRequested(m_hStopEvent) || ShouldCancelCurrent())
		{
			strMessage = _T("작업이 취소되었습니다.");
			eInterruptResult = DRIVE_TASK_RESULT::Canceled;
			return FALSE;
		}

		if (HasReachedEndUsage(task))
		{
			strMessage = _T("종료 사용률 조건에 도달했습니다.");
			return FALSE;
		}

		return TRUE;
	};

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

		const BOOL bSucceeded = task.eType == DRIVE_TASK_TYPE::MoveFiles
			? driveFileManager.MovePath(vecItems[i], strDestPath, continueWork)
			: driveFileManager.RemovePath(vecItems[i], continueWork);
		if (!bSucceeded)
		{
			if (!strMessage.IsEmpty())
			{
				return eInterruptResult;
			}

			strMessage = task.eType == DRIVE_TASK_TYPE::MoveFiles
				? _T("파일 이동 중 오류가 발생했습니다.")
				: _T("파일 삭제 중 오류가 발생했습니다.");
			return DRIVE_TASK_RESULT::Failed;
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

BOOL CDriveTaskWorker::MatchesTask(const DRIVE_TASK& task, LPCTSTR lpszTemplateName) const
{
	CString strTemplateName;
	if (lpszTemplateName != nullptr)
	{
		strTemplateName = lpszTemplateName;
		strTemplateName.Trim();
	}

	return !strTemplateName.IsEmpty()
		&& task.strTemplateName.CompareNoCase(strTemplateName) == 0;
}

BOOL CDriveTaskWorker::HasQueuedTask(LPCTSTR lpszTemplateName) const
{
	for (const DRIVE_TASK& task : m_queue)
	{
		if (MatchesTask(task, lpszTemplateName))
		{
			return TRUE;
		}
	}

	return FALSE;
}

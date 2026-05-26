#include "pch.h"
#include "CTestDlg.h"
#include "Manager/CDriveManager.h"
#include "Manager/FileSystemUtil.h"


namespace
{
	constexpr int BYTES_PER_MB = 1024 * 1024;
	constexpr int FILE_SIZE_BASE_MB = 10;
	constexpr int TEST_BUFFER_SIZE = 1024 * 1024;

}

CTestDlg::CTestDlg(CWnd* pParent)
	: CDialogEx(IDD_TEST_DIALOG, pParent)
{
}

CTestDlg::~CTestDlg()
{
	if (m_hCancelEvent != nullptr)
	{
		SetEvent(m_hCancelEvent);
	}

	if (m_pThread != nullptr)
	{
		WaitForSingleObject(m_pThread->m_hThread, INFINITE);
		delete m_pThread;
		m_pThread = nullptr;
	}

	if (m_hCancelEvent != nullptr)
	{
		CloseHandle(m_hCancelEvent);
		m_hCancelEvent = nullptr;
	}
}

BOOL CTestDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	LoadDriveCombos();
	SetDlgItemInt(IDC_EDIT_TEST_SIZE_GB, 1, FALSE);
	SetRunningState(FALSE);
	return TRUE;
}

void CTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTestDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_TEST_START, &CTestDlg::OnBnClickedBtnTestStart)
	ON_BN_CLICKED(IDC_BTN_TEST_CANCEL, &CTestDlg::OnBnClickedBtnTestCancel)
	ON_BN_CLICKED(IDC_CHECK_TEST_DELETE_ONLY, &CTestDlg::OnBnClickedCheckDeleteOnly)
	ON_MESSAGE(WM_TEST_FINISHED, &CTestDlg::OnTestFinished)
END_MESSAGE_MAP()

void CTestDlg::OnCancel()
{
	OnBnClickedBtnTestCancel();
}

void CTestDlg::OnBnClickedBtnTestStart()
{
	if (m_pThread != nullptr)
	{
		return;
	}

	TEST_CONTEXT* pContext = new TEST_CONTEXT;
	CString strErrorMessage;
	if (!ReadInputs(*pContext, strErrorMessage))
	{
		delete pContext;
		MessageBox(strErrorMessage, _T("테스트"), MB_OK | MB_ICONWARNING);
		return;
	}

	m_hCancelEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (m_hCancelEvent == nullptr)
	{
		delete pContext;
		MessageBox(_T("취소 이벤트를 생성할 수 없습니다."), _T("테스트"), MB_OK | MB_ICONERROR);
		return;
	}

	pContext->pDlg = this;
	m_pThread = AfxBeginThread(ThreadProc, pContext, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (m_pThread == nullptr)
	{
		delete pContext;
		CloseHandle(m_hCancelEvent);
		m_hCancelEvent = nullptr;
		MessageBox(_T("테스트 스레드를 시작할 수 없습니다."), _T("테스트"), MB_OK | MB_ICONERROR);
		return;
	}

	m_pThread->m_bAutoDelete = FALSE;
	SetRunningState(TRUE);
	m_pThread->ResumeThread();
}

void CTestDlg::OnBnClickedBtnTestCancel()
{
	if (m_pThread != nullptr)
	{
		if (m_hCancelEvent != nullptr)
		{
			SetEvent(m_hCancelEvent);
		}
		return;
	}

	EndDialog(IDCANCEL);
}

void CTestDlg::OnBnClickedCheckDeleteOnly()
{
	UpdateMoveDriveControlState();
}

LRESULT CTestDlg::OnTestFinished(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);

	if (m_pThread != nullptr)
	{
		WaitForSingleObject(m_pThread->m_hThread, INFINITE);
		delete m_pThread;
		m_pThread = nullptr;
	}

	if (m_hCancelEvent != nullptr)
	{
		CloseHandle(m_hCancelEvent);
		m_hCancelEvent = nullptr;
	}

	TEST_RESULT* pResult = reinterpret_cast<TEST_RESULT*>(lParam);
	if (pResult != nullptr)
	{
		SetRunningState(FALSE);
		MessageBox(BuildResultMessage(*pResult), _T("테스트 결과"),
			MB_OK | (pResult->bFailed ? MB_ICONWARNING : MB_ICONINFORMATION));
		delete pResult;
	}

	return 0;
}

UINT CTestDlg::ThreadProc(LPVOID pParam)
{
	TEST_CONTEXT* pContext = reinterpret_cast<TEST_CONTEXT*>(pParam);
	if (pContext == nullptr || pContext->pDlg == nullptr)
	{
		delete pContext;
		return 0;
	}

	CTestDlg* pDlg = pContext->pDlg;
	TEST_RESULT* pResult = new TEST_RESULT(pDlg->RunTest(*pContext));
	delete pContext;

	if (!pDlg->PostMessage(WM_TEST_FINISHED, 0, reinterpret_cast<LPARAM>(pResult)))
	{
		delete pResult;
	}

	return 0;
}


CTestDlg::TEST_RESULT CTestDlg::FinishTestResult(TEST_RESULT result) const
{
	CleanupGeneratedFolderTree(result.strSourceRoot);
	CleanupGeneratedFolderTree(result.strDestRoot);
	return result;
}
CTestDlg::TEST_RESULT CTestDlg::RunTest(const TEST_CONTEXT& context)
{
	TEST_RESULT result;
	const CString strFolderName = BuildTestFolderName();
	result.strSourceRoot = AutoMoveFileSystem::CombinePath(AutoMoveFileSystem::CombinePath(AutoMoveFileSystem::BuildDriveRootPath(context.strTargetDrive), _T("AutoMoveTest")), strFolderName);
	if (!context.bDeleteOnly)
	{
		result.strDestRoot = AutoMoveFileSystem::CombinePath(AutoMoveFileSystem::CombinePath(AutoMoveFileSystem::BuildDriveRootPath(context.strMoveDrive), _T("AutoMoveTestMoved")), strFolderName);
	}

	CDriveFileManager driveFileManager;
	const ULONGLONG ullTargetBytes = static_cast<ULONGLONG>(context.nTestGb) * 1024ULL * 1024ULL * 1024ULL;
	ULONGLONG ullCreatedBytes = 0;
	int nFileIndex = 0;

	if (!AutoMoveFileSystem::EnsureDirectoryExists(result.strSourceRoot))
	{
		result.bFailed = TRUE;
		result.strMessage = _T("테스트 폴더를 생성할 수 없습니다.");
		return FinishTestResult(result);
	}

	DWORD dwStart = GetTickCount();
	while (ullCreatedBytes < ullTargetBytes)
	{
		if (IsCancelRequested())
		{
			result.bCanceled = TRUE;
			result.strMessage = _T("파일 생성 중 테스트가 취소되었습니다.");
			return FinishTestResult(result);
		}

		const int nSizeMb = FILE_SIZE_BASE_MB + ((nFileIndex % 5) - 2);
		const ULONGLONG ullFileSize = static_cast<ULONGLONG>(nSizeMb) * BYTES_PER_MB;


		CString strFileName;
		strFileName.Format(_T("test_%06d.bin"), nFileIndex + 1);
		if (!WriteDeterministicFile(AutoMoveFileSystem::CombinePath(result.strSourceRoot, strFileName), ullFileSize, nFileIndex))
		{
			if (IsCancelRequested())
			{
				result.bCanceled = TRUE;
				result.strMessage = _T("파일 생성 중 테스트가 취소되었습니다.");
				return FinishTestResult(result);
			}

			result.bFailed = TRUE;
			result.strMessage = _T("테스트 파일을 생성할 수 없습니다.");
			return FinishTestResult(result);
		}

		ullCreatedBytes += ullFileSize;
		++nFileIndex;
		++result.nCreatedFiles;
	}
	result.dwCreateMs = GetTickCount() - dwStart;

	if (IsCancelRequested())
	{
		result.bCanceled = TRUE;
		result.strMessage = context.bDeleteOnly ? _T("파일 삭제 전에 테스트가 취소되었습니다.") : _T("파일 이동 전에 테스트가 취소되었습니다.");
		return FinishTestResult(result);
	}

	int nDeleteTargetFiles = result.nCreatedFiles;
	CString strDeleteRoot = result.strSourceRoot;
	if (context.bDeleteOnly)
	{
		result.nMovedFiles = 0;
	}
	else
	{
		const std::vector<CString> vecMoveItems = driveFileManager.FindMoveItems(result.strSourceRoot);
		dwStart = GetTickCount();
		for (int i = 0; i < static_cast<int>(vecMoveItems.size()); ++i)
		{
			if (IsCancelRequested())
			{
				break;
			}

			driveFileManager.MovePath(vecMoveItems[i], result.strDestRoot);
		}
		result.dwMoveMs = GetTickCount() - dwStart;
		result.nMovedFiles = CountFiles(result.strDestRoot);
		if (IsCancelRequested())
		{
			result.bCanceled = TRUE;
			result.strMessage = _T("파일 이동 중 테스트가 취소되었습니다.");
			return FinishTestResult(result);
		}

		if (result.nMovedFiles != result.nCreatedFiles)
		{
			result.bFailed = TRUE;
			result.strMessage = _T("일부 테스트 파일을 이동할 수 없습니다.");
			return FinishTestResult(result);
		}
		CleanupGeneratedFolderTree(result.strSourceRoot);

		if (IsCancelRequested())
		{
			result.bCanceled = TRUE;
			result.strMessage = _T("이동된 파일 삭제 전에 테스트가 취소되었습니다.");
			return FinishTestResult(result);
		}

		nDeleteTargetFiles = result.nMovedFiles;
		strDeleteRoot = result.strDestRoot;
	}
	dwStart = GetTickCount();
	const std::vector<CString> vecDeleteItems = driveFileManager.FindFiles(strDeleteRoot);
	for (int i = 0; i < static_cast<int>(vecDeleteItems.size()); ++i)
	{
		if (IsCancelRequested())
		{
			break;
		}

		driveFileManager.RemovePath(vecDeleteItems[i]);
	}
	result.dwDeleteMs = GetTickCount() - dwStart;
	result.nDeletedFiles = nDeleteTargetFiles - CountFiles(strDeleteRoot);

	if (IsCancelRequested())
	{
		result.bCanceled = TRUE;
		result.strMessage = context.bDeleteOnly ? _T("파일 삭제 중 테스트가 취소되었습니다.") : _T("이동된 파일 삭제 중 테스트가 취소되었습니다.");
		return FinishTestResult(result);
	}

	if (result.nDeletedFiles != nDeleteTargetFiles)
	{
		result.bFailed = TRUE;
		result.strMessage = context.bDeleteOnly ? _T("일부 테스트 파일을 삭제할 수 없습니다.") : _T("일부 이동된 테스트 파일을 삭제할 수 없습니다.");
		return FinishTestResult(result);
	}

	result.strMessage = context.bDeleteOnly ? _T("삭제 테스트가 완료되었습니다.") : _T("테스트가 완료되었습니다.");
	return FinishTestResult(result);
}

void CTestDlg::LoadDriveCombos()
{
	CDriveManager driveManager;
	driveManager.LoadAvailableDrives();
	const std::vector<CString> vecDriveNames = driveManager.GetDriveNames();

	CComboBox* pTargetCombo = reinterpret_cast<CComboBox*>(GetDlgItem(IDC_COMBO_TEST_TARGET_DRIVE));
	CComboBox* pMoveCombo = reinterpret_cast<CComboBox*>(GetDlgItem(IDC_COMBO_TEST_MOVE_DRIVE));
	if (pTargetCombo == nullptr || pMoveCombo == nullptr)
	{
		return;
	}

	for (int i = 0; i < static_cast<int>(vecDriveNames.size()); ++i)
	{
		CString strName;
		strName.Format(_T("%s:"), static_cast<LPCTSTR>(vecDriveNames[i]));
		pTargetCombo->AddString(strName);
		pMoveCombo->AddString(strName);
	}

	if (!vecDriveNames.empty())
	{
		pTargetCombo->SetCurSel(0);
		pMoveCombo->SetCurSel(vecDriveNames.size() > 1 ? 1 : 0);
	}
}

BOOL CTestDlg::ReadInputs(TEST_CONTEXT& context, CString& strErrorMessage) const
{
	CComboBox* pTargetCombo = reinterpret_cast<CComboBox*>(GetDlgItem(IDC_COMBO_TEST_TARGET_DRIVE));
	CComboBox* pMoveCombo = reinterpret_cast<CComboBox*>(GetDlgItem(IDC_COMBO_TEST_MOVE_DRIVE));
	if (pTargetCombo == nullptr || pMoveCombo == nullptr
		|| pTargetCombo->GetCurSel() == CB_ERR)
	{
		strErrorMessage = _T("드라이브를 선택해야 합니다.");
		return FALSE;
	}

	context.bDeleteOnly = IsDlgButtonChecked(IDC_CHECK_TEST_DELETE_ONLY) == BST_CHECKED;
	pTargetCombo->GetLBText(pTargetCombo->GetCurSel(), context.strTargetDrive);
	if (!context.bDeleteOnly)
	{
		if (pMoveCombo->GetCurSel() == CB_ERR)
		{
			strErrorMessage = _T("드라이브를 선택해야 합니다.");
			return FALSE;
		}

		pMoveCombo->GetLBText(pMoveCombo->GetCurSel(), context.strMoveDrive);
	}
	context.strTargetDrive.TrimRight(_T(":\\"));
	context.strMoveDrive.TrimRight(_T(":\\"));

	BOOL bTranslated = FALSE;
	context.nTestGb = GetDlgItemInt(IDC_EDIT_TEST_SIZE_GB, &bTranslated, FALSE);
	if (!bTranslated || context.nTestGb <= 0)
	{
		strErrorMessage = _T("테스트 용량은 1GB 이상의 값이어야 합니다.");
		return FALSE;
	}

	ULARGE_INTEGER freeBytesAvailable = {};
	ULARGE_INTEGER totalBytes = {};
	ULARGE_INTEGER totalFreeBytes = {};
	if (!GetDiskFreeSpaceEx(AutoMoveFileSystem::BuildDriveRootPath(context.strTargetDrive), &freeBytesAvailable, &totalBytes, &totalFreeBytes))
	{
		strErrorMessage = _T("대상 드라이브의 여유 공간을 확인할 수 없습니다.");
		return FALSE;
	}

	const ULONGLONG ullRequiredBytes = static_cast<ULONGLONG>(context.nTestGb) * 1024ULL * 1024ULL * 1024ULL;
	if (freeBytesAvailable.QuadPart <= ullRequiredBytes)
	{
		strErrorMessage = _T("대상 드라이브의 여유 공간이 부족합니다.");
		return FALSE;
	}

	if (!context.bDeleteOnly && context.strTargetDrive.CompareNoCase(context.strMoveDrive) != 0)
	{
		freeBytesAvailable.QuadPart = 0;
		totalBytes.QuadPart = 0;
		totalFreeBytes.QuadPart = 0;
		if (!GetDiskFreeSpaceEx(AutoMoveFileSystem::BuildDriveRootPath(context.strMoveDrive), &freeBytesAvailable, &totalBytes, &totalFreeBytes))
		{
			strErrorMessage = _T("이동 드라이브의 여유 공간을 확인할 수 없습니다.");
			return FALSE;
		}

		if (freeBytesAvailable.QuadPart <= ullRequiredBytes)
		{
			strErrorMessage = _T("이동 드라이브의 여유 공간이 부족합니다.");
			return FALSE;
		}
	}

	return TRUE;
}
void CTestDlg::SetRunningState(BOOL bRunning)
{
	CWnd* pStart = GetDlgItem(IDC_BTN_TEST_START);
	if (pStart != nullptr)
	{
		pStart->EnableWindow(!bRunning);
	}

	CWnd* pDeleteOnly = GetDlgItem(IDC_CHECK_TEST_DELETE_ONLY);
	if (pDeleteOnly != nullptr)
	{
		pDeleteOnly->EnableWindow(!bRunning);
	}

	CWnd* pCancel = GetDlgItem(IDC_BTN_TEST_CANCEL);
	if (pCancel != nullptr)
	{
		pCancel->SetWindowText(bRunning ? _T("취소") : _T("닫기"));
	}

	UpdateMoveDriveControlState();
}

void CTestDlg::UpdateMoveDriveControlState()
{
	CWnd* pMoveCombo = GetDlgItem(IDC_COMBO_TEST_MOVE_DRIVE);
	if (pMoveCombo != nullptr)
	{
		pMoveCombo->EnableWindow(m_pThread == nullptr
			&& IsDlgButtonChecked(IDC_CHECK_TEST_DELETE_ONLY) != BST_CHECKED);
	}
}
BOOL CTestDlg::IsCancelRequested() const
{
	return m_hCancelEvent != nullptr
		&& WaitForSingleObject(m_hCancelEvent, 0) == WAIT_OBJECT_0;
}

CString CTestDlg::BuildResultMessage(const TEST_RESULT& result) const
{
	const CString strDestRoot = result.strDestRoot.IsEmpty() ? _T("-") : result.strDestRoot;

	CString strMessage;
	strMessage.Format(_T("%s\r\n\r\n생성 경로: %s\r\n이동 경로: %s\r\n\r\n생성 파일 수: %d\r\n이동 파일 수: %d\r\n삭제 파일 수: %d\r\n\r\n생성 Tact time: %.3f초\r\n이동 Tact time: %.3f초\r\n삭제 Tact time: %.3f초"),
		static_cast<LPCTSTR>(result.strMessage),
		static_cast<LPCTSTR>(result.strSourceRoot),
		static_cast<LPCTSTR>(strDestRoot),
		result.nCreatedFiles,
		result.nMovedFiles,
		result.nDeletedFiles,
		static_cast<double>(result.dwCreateMs) / 1000.0,
		static_cast<double>(result.dwMoveMs) / 1000.0,
		static_cast<double>(result.dwDeleteMs) / 1000.0);
	return strMessage;
}

CString CTestDlg::BuildTestFolderName() const
{
	SYSTEMTIME now;
	GetLocalTime(&now);

	static constexpr LPCTSTR ALPHABET = _T("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	DWORD dwSeed = static_cast<DWORD>(now.wMilliseconds)
		^ (static_cast<DWORD>(now.wSecond) << 8)
		^ (static_cast<DWORD>(now.wMinute) << 16)
		^ GetTickCount();

	CString strToken;
	for (int i = 0; i < 8; ++i)
	{
		dwSeed = dwSeed * 1664525UL + 1013904223UL;
		strToken += ALPHABET[dwSeed % 36];
	}

	CString strName;
	strName.Format(_T("%04u%02u%02u\\%s"), now.wYear, now.wMonth, now.wDay, static_cast<LPCTSTR>(strToken));
	return strName;
}


BOOL CTestDlg::WriteDeterministicFile(const CString& strFilePath, ULONGLONG ullFileSize, int nFileIndex) const
{
	CFile file;
	if (!file.Open(strFilePath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
	{
		return FALSE;
	}

	std::vector<BYTE> vecBuffer(TEST_BUFFER_SIZE);
	for (int i = 0; i < TEST_BUFFER_SIZE; ++i)
	{
		vecBuffer[i] = static_cast<BYTE>((i + nFileIndex * 31) & 0xFF);
	}

	ULONGLONG ullWritten = 0;
	while (ullWritten < ullFileSize)
	{
		if (IsCancelRequested())
		{
			file.Close();
			return FALSE;
		}

		const ULONGLONG ullRemaining = ullFileSize - ullWritten;
		const UINT nWriteSize = static_cast<UINT>(ullRemaining < TEST_BUFFER_SIZE ? ullRemaining : TEST_BUFFER_SIZE);
		file.Write(vecBuffer.data(), nWriteSize);
		ullWritten += nWriteSize;
	}

	file.Close();
	return TRUE;
}

int CTestDlg::CountFiles(const CString& strRootPath) const
{
	int nCount = 0;
	std::vector<CString> vecStack;
	vecStack.push_back(strRootPath);

	while (!vecStack.empty())
	{
		const CString strDirectory = vecStack.back();
		vecStack.pop_back();

		CFileFind finder;
		BOOL bWorking = finder.FindFile(AutoMoveFileSystem::CombinePath(strDirectory, _T("*")));
		while (bWorking)
		{
			bWorking = finder.FindNextFile();
			if (finder.IsDots())
			{
				continue;
			}

			const CString strPath = finder.GetFilePath();
			const DWORD dwAttributes = GetFileAttributes(strPath);
			if (dwAttributes == INVALID_FILE_ATTRIBUTES
				|| AutoMoveFileSystem::IsReparsePoint(dwAttributes))
			{
				continue;
			}

			if (finder.IsDirectory())
			{
				vecStack.push_back(strPath);
			}
			else
			{
				++nCount;
			}
		}
		finder.Close();
	}

	return nCount;
}

void CTestDlg::CleanupGeneratedFolderTree(const CString& strRootPath) const
{
	if (!IsGeneratedTestRoot(strRootPath))
	{
		return;
	}

	CDriveFileManager driveFileManager;
	const std::vector<CString> vecItems = driveFileManager.FindFiles(strRootPath);
	driveFileManager.RemoveFiles(vecItems);

	RemoveDirectory(strRootPath);

	CString strPath = strRootPath;
	strPath.TrimRight(_T("\\/"));
	for (int i = 0; i < 2; ++i)
	{
		const int nBackslash = strPath.ReverseFind(_T('\\'));
		const int nSlash = strPath.ReverseFind(_T('/'));
		const int nSeparator = max(nBackslash, nSlash);
		if (nSeparator <= 2)
		{
			return;
		}

		strPath = strPath.Left(nSeparator);
		RemoveDirectory(strPath);
	}
}
BOOL CTestDlg::IsGeneratedTestRoot(const CString& strRootPath) const
{
	CString strPath = strRootPath;
	strPath.Trim();
	strPath.TrimRight(_T("\\/"));
	if (strPath.GetLength() < 4)
	{
		return FALSE;
	}

	return strPath.Find(_T("\\AutoMoveTest\\")) >= 0
		|| strPath.Find(_T("\\AutoMoveTestMoved\\")) >= 0;
}
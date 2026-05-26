#pragma once

#include <afxdialogex.h>
#include "resource.h"
#include <vector>

constexpr UINT WM_TEST_FINISHED = WM_APP + 40;

class CTestDlg : public CDialogEx
{
public:
	CTestDlg(CWnd* pParent = nullptr);
	virtual ~CTestDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TEST_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnCancel();

	DECLARE_MESSAGE_MAP()

private:
	struct TEST_RESULT
	{
		BOOL bCanceled = FALSE;
		BOOL bFailed = FALSE;
		CString strMessage;
		CString strSourceRoot;
		CString strDestRoot;
		int nCreatedFiles = 0;
		int nMovedFiles = 0;
		int nDeletedFiles = 0;
		DWORD dwCreateMs = 0;
		DWORD dwMoveMs = 0;
		DWORD dwDeleteMs = 0;
	};

	struct TEST_CONTEXT
	{
		CTestDlg* pDlg = nullptr;
		CString strTargetDrive;
		CString strMoveDrive;
		int nTestGb = 0;
		BOOL bDeleteOnly = FALSE;
	};

	afx_msg void OnBnClickedBtnTestStart();
	afx_msg void OnBnClickedBtnTestCancel();
	afx_msg void OnBnClickedCheckDeleteOnly();
	afx_msg LRESULT OnTestFinished(WPARAM wParam, LPARAM lParam);

	static UINT ThreadProc(LPVOID pParam);
	TEST_RESULT RunTest(const TEST_CONTEXT& context);
	TEST_RESULT FinishTestResult(TEST_RESULT result) const;

	void LoadDriveCombos();
	BOOL ReadInputs(TEST_CONTEXT& context, CString& strErrorMessage) const;
	void SetRunningState(BOOL bRunning);
	void UpdateMoveDriveControlState();
	BOOL IsCancelRequested() const;
	CString BuildResultMessage(const TEST_RESULT& result) const;

	CString BuildTestFolderName() const;
	BOOL WriteDeterministicFile(const CString& strFilePath, ULONGLONG ullFileSize, int nFileIndex) const;
	int CountFiles(const CString& strRootPath) const;
	void CleanupGeneratedFolderTree(const CString& strRootPath) const;
	BOOL IsGeneratedTestRoot(const CString& strRootPath) const;

	CWinThread* m_pThread = nullptr;
	HANDLE m_hCancelEvent = nullptr;
};

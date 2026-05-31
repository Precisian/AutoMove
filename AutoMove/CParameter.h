#pragma once
#include <vector>

class CParameter
{
public:
	static constexpr LPCTSTR DEFAULT_INI_PATH = _T(".\\System.ini");
	static constexpr LPCTSTR TEMPLATE_SECTION = _T("Template");

	struct TemplateKey
	{
		static constexpr LPCTSTR NAME = _T("Name");
		static constexpr LPCTSTR ORIGIN_PATH = _T("OriginPath");
		static constexpr LPCTSTR DEST_PATH = _T("DestPath");
		static constexpr LPCTSTR ENABLE_MOVE = _T("EnableMove");
		static constexpr LPCTSTR BOOT_START = _T("BootStart");
		static constexpr LPCTSTR DRIVE_NAME = _T("DriveName");
		static constexpr LPCTSTR LIMIT_MODE = _T("LimitMode");
		static constexpr LPCTSTR LIMIT_VALUE = _T("LimitValue");
		static constexpr LPCTSTR END_VALUE = _T("EndValue");
		static constexpr LPCTSTR SCHEDULE_DAYS = _T("ScheduleDays");
		static constexpr LPCTSTR SCHEDULE_TIME = _T("ScheduleTime");
		static constexpr LPCTSTR LIMIT_MODE_STORAGE = _T("Storage");
		static constexpr LPCTSTR LIMIT_MODE_SCHEDULE = _T("Schedule");
	};

	struct PARAM_TEMPLATE
	{
		CString strName;
		CString strOriginPath;
		CString strDestPath;
		BOOL bEnableMove = FALSE;
		BOOL bBootStart = FALSE;
		CString strDriveName;
		CString strLimitMode = TemplateKey::LIMIT_MODE_STORAGE;
		CString strLimitValue;
		CString strEndValue;
		CString strScheduleDays;
		CString strScheduleTime;
		CString strLastScheduleRunDate;
	};

	CParameter();
	virtual ~CParameter() = default;

	virtual void InitDefault();
	BOOL Load();
	BOOL Save();

	void AddTemplate(LPCTSTR lpszName);
	void ClearTemplate();

	void SetIniPath(LPCTSTR lpszIniPath);
	CString GetIniPath() const;

	static BOOL IsScheduleLimitMode(const PARAM_TEMPLATE& paramTemplate);

	// Parameter member variables
	std::vector<PARAM_TEMPLATE> m_vecTemplate;


private:
	BOOL LoadTemplate();
	BOOL SaveTemplate();
	void LoadTemplateNames(std::vector<CString>& vecTemplateNames) const;
	BOOL DeleteTemplateSection(LPCTSTR lpszName) const;
	BOOL FormatIniFile() const;
	PARAM_TEMPLATE* FindTemplate(LPCTSTR lpszName);
	CString GetTemplateSection(LPCTSTR lpszName) const;
	BOOL WriteString(LPCTSTR lpszSection, LPCTSTR lpszKey, LPCTSTR lpszValue) const;

	CString m_strIniPath;
};

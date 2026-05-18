#pragma once
#include <vector>

class CParameter
{
public:
	static constexpr LPCTSTR DEFAULT_INI_PATH = _T(".\\System.ini");
	static constexpr LPCTSTR TEMPLATE_SECTION = _T("Templete");

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

	enum TEMPLATE_RUNTIME_STATE
	{
		TEMPLATE_STATE_IDLE,
		TEMPLATE_STATE_WAITING,
		TEMPLATE_STATE_WORKING
	};

	struct PARAM_TEMPLATE_VALUE
	{
		CString strKey;
		CString strValue;
	};

	struct PARAM_TEMPLATE
	{
		CString strName;
		std::vector<PARAM_TEMPLATE_VALUE> vecValue;
		TEMPLATE_RUNTIME_STATE eRuntimeState = TEMPLATE_STATE_IDLE;
	};

	CParameter();
	virtual ~CParameter() = default;

	virtual void InitDefault();
	BOOL Load();
	BOOL Save();

	void AddTemplate(LPCTSTR lpszName);
	void AddTemplateParam(LPCTSTR lpszName, LPCTSTR lpszKey, LPCTSTR lpszValue);
	void ClearTemplate();

	void SetIniPath(LPCTSTR lpszIniPath);
	CString GetIniPath() const;

	static CString GetTemplateValue(const PARAM_TEMPLATE& paramTemplate,
		LPCTSTR lpszKey, LPCTSTR lpszDefault = _T(""));
	static void SetTemplateValue(PARAM_TEMPLATE& paramTemplate,
		LPCTSTR lpszKey, LPCTSTR lpszValue);
	static void AddTemplateValue(PARAM_TEMPLATE& paramTemplate,
		LPCTSTR lpszKey, const CString& strValue);
	static BOOL IsScheduleLimitMode(const PARAM_TEMPLATE& paramTemplate);

	// Parameter member variables
	std::vector<PARAM_TEMPLATE> m_vecTemplate;


private:
	enum PARAM_MODE
	{
		PARAM_LOAD,
		PARAM_SAVE
	};

	BOOL LoadTemplate();
	BOOL SaveTemplate();
	void LoadTemplateNames(std::vector<CString>& vecTemplateNames) const;
	BOOL DeleteTemplateSection(LPCTSTR lpszName) const;
	BOOL FormatIniFile() const;
	PARAM_TEMPLATE* FindTemplate(LPCTSTR lpszName);
	CString GetTemplateSection(LPCTSTR lpszName) const;
	BOOL WriteString(LPCTSTR lpszSection, LPCTSTR lpszKey, LPCTSTR lpszValue) const;

	CString m_strIniPath;
	PARAM_MODE m_eMode;
};
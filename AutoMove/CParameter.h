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
		static constexpr LPCTSTR SCHEDULE_DAYS = _T("ScheduleDays");
		static constexpr LPCTSTR SCHEDULE_TIME = _T("ScheduleTime");
		static constexpr LPCTSTR LIMIT_MODE_STORAGE = _T("Storage");
		static constexpr LPCTSTR LIMIT_MODE_SCHEDULE = _T("Schedule");
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

protected:
	template<typename T, typename TDefault>
	BOOL AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, T& value, const TDefault& defaultValue);

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

template<typename T>
struct ParamValueCodec;

template<>
struct ParamValueCodec<int>
{
	static CString ToString(int value)
	{
		CString strValue;
		strValue.Format(_T("%d"), value);
		return strValue;
	}

	static int FromString(const CString& strValue)
	{
		return _ttoi(strValue);
	}
};

template<>
struct ParamValueCodec<UINT>
{
	static CString ToString(UINT value)
	{
		CString strValue;
		strValue.Format(_T("%u"), value);
		return strValue;
	}

	static UINT FromString(const CString& strValue)
	{
		return static_cast<UINT>(_ttoi(strValue));
	}
};

template<>
struct ParamValueCodec<double>
{
	static CString ToString(double value)
	{
		CString strValue;
		strValue.Format(_T("%.15g"), value);
		return strValue;
	}

	static double FromString(const CString& strValue)
	{
		return _tcstod(strValue, nullptr);
	}
};

template<>
struct ParamValueCodec<CString>
{
	static CString ToString(const CString& value)
	{
		return value;
	}

	static CString FromString(const CString& strValue)
	{
		return strValue;
	}
};

template<typename T, typename TDefault>
BOOL CParameter::AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, T& value, const TDefault& defaultValue)
{
	if (m_eMode == PARAM_LOAD)
	{
		const CString strDefault = ParamValueCodec<T>::ToString(defaultValue);
		CString strValue;
		GetPrivateProfileString(lpszSection, lpszKey, strDefault,
			strValue.GetBuffer(4096), 4096, m_strIniPath);
		strValue.ReleaseBuffer();

		value = ParamValueCodec<T>::FromString(strValue);
		return TRUE;
	}

	return WriteString(lpszSection, lpszKey, ParamValueCodec<T>::ToString(value));
}

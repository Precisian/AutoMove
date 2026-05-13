#pragma once
#include <vector>

class CParameter
{
public:
	static constexpr LPCTSTR DEFAULT_INI_PATH = _T(".\\System.ini");
	static constexpr LPCTSTR TEMPLATE_SECTION = _T("Templete");

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
	virtual BOOL Add();

	void AddTemplate(LPCTSTR lpszName);
	void AddTemplateParam(LPCTSTR lpszName, LPCTSTR lpszKey, LPCTSTR lpszValue);
	void ClearTemplate();

	void SetIniPath(LPCTSTR lpszIniPath);
	CString GetIniPath() const;

	// Parameter member variables
	// int m_nMoveDelay;
	// CString m_strSourcePath;
	// BOOL m_bUseAutoMove;
	std::vector<PARAM_TEMPLATE> m_vecTemplate;

protected:
	BOOL AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, int& nValue, int nDefault);
	BOOL AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, UINT& nValue, UINT nDefault);
	BOOL AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, double& dValue, double dDefault);
	BOOL AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, CString& strValue, LPCTSTR lpszDefault);

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

#pragma once

class CLogManager
{
public:
	enum LOG_TYPE
	{
		LOG_SYSTEM,
		LOG_OPERATION
	};

	static void Write(LOG_TYPE eLogType, const CString& strMessage);
	static void Clear(LOG_TYPE eLogType);
	static CString GetLogPath(LOG_TYPE eLogType);

private:
	static CString GetLogDirectory();
	static CString GetLogTypeName(LOG_TYPE eLogType);
	static CString GetTimestamp();
	static CString BuildLogLine(LOG_TYPE eLogType, const CString& strMessage);
	static BOOL EnsureLogDirectory();
	static BOOL WriteUtf8Line(const CString& strPath, const CString& strLine);
};

#ifndef COMPILED_DATE_TIME_H
#define COMPILED_DATE_TIME_H

#ifdef _WIN32
#ifndef _CONSOLE

#include <atlcomtime.h>

static tstring getCompileDate(const TCHAR *format = _T("%Y-%m-%d"))
{
	COleDateTime dt;
	dt.ParseDateTime(_T(__DATE__), LOCALE_NOUSEROVERRIDE, 1033);
	return dt.Format(format).GetString();
}
		
static tstring getCompileTime(const TCHAR *format = _T("%H-%M-%S"))
{
	COleDateTime dt;
	dt.ParseDateTime(_T(__TIME__), LOCALE_NOUSEROVERRIDE, 1033);
	return dt.Format(format).GetString();
}

#endif // _CONSOLE
#endif // _WIN32

#endif // COMPILED_DATE_TIME_H

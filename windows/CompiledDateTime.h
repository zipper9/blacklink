#ifndef COMPILED_DATE_TIME_H
#define COMPILED_DATE_TIME_H

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

#endif // COMPILED_DATE_TIME_H

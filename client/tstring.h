#ifndef TSTRING_H_
#define TSTRING_H_

#include <string>

using std::string;
using std::wstring;

#ifdef _UNICODE
typedef wstring tstring;
typedef wchar_t tchar_t;
#else
typedef string tstring;
typedef char tchar_t;
#endif

#endif

#include "stdinc.h"
#include "BaseUtil.h"
#include "StrUtil.h"

const string Util::emptyString;
const wstring Util::emptyStringW;
const tstring Util::emptyStringT;
const vector<uint8_t> Util::emptyByteVector;

#ifndef _WIN32

#ifdef HAVE_STRERROR_R

// Choose XSI or GNU version of strerror_r
static inline char* strErrorOverload(int result, char* buf) noexcept { return result ? nullptr : buf; }
static inline char* strErrorOverload(char* result, char*) noexcept { return result; }

#else

#include "Locks.h"
static CriticalSection csError;

#endif

#endif // _WIN32

string Util::translateError(unsigned error) noexcept
{
	string tmp;
#ifdef _WIN32
	LPTSTR lpMsgBuf = nullptr;
	DWORD chars = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		error,
#ifdef _CONSOLE
		MAKELANGID(LANG_NEUTRAL, SUBLANG_ENGLISH_US), // US
#else
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
#endif
		(LPTSTR) &lpMsgBuf, 0, nullptr);
	if (chars != 0)
	{
		tmp = Text::fromT(lpMsgBuf);
		LocalFree(lpMsgBuf);
		string::size_type i = 0;
		while ((i = tmp.find_first_of("\r\n", i)) != string::npos)
		{
			tmp.erase(i, 1);
		}
	}
#else // _WIN32
#ifdef HAVE_STRERROR_R
	char buf[256];
	char* result = strErrorOverload(strerror_r(error, buf, sizeof(buf)), buf);
	if (!result)
	{
		sprintf(buf, "Error %u", error);
		return buf;
	}
	tmp = result;
#else
	csError.lock();
	tmp = strerror(error);
	csError.unlock();
#endif
#endif // _WIN32
	tmp += " (";
	tmp += Util::toString(error);
	tmp += ')';
	return tmp;
}

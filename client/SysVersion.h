#ifndef SYS_VERSION_H_
#define SYS_VERSION_H_

#include "typedefs.h"

#ifdef _WIN32
#include "w.h"
#endif

#if defined(_MSC_VER)

#if defined(_M_IX86)
#define COMPILED_ARCH_STRING "x86"
#elif defined(_M_X64)
#define COMPILED_ARCH_STRING "x86_64"
#elif defined(_M_ARM)
#define COMPILED_ARCH_STRING "ARM"
#elif defined(_M_ARM64)
#define COMPILED_ARCH_STRING "ARM64"
#endif

#elif defined(__GNUC__)

#if defined(__i386__)
#define COMPILED_ARCH_STRING "x86"
#elif defined(__amd64__)
#define COMPILED_ARCH_STRING "x86_64"
#elif defined(__arm__)
#define COMPILED_ARCH_STRING "ARM"
#elif defined(__aarch64__)
#define COMPILED_ARCH_STRING "ARM64"
#endif

#endif

#ifndef COMPILED_ARCH_STRING
#define COMPILED_ARCH_STRING "-"
#endif

namespace SysVersion
{
	void initialize();

#ifdef _WIN32
	extern unsigned flags;
	extern OSVERSIONINFOEX osvi;
	extern SYSTEM_INFO sysInfo;

	enum Flags
	{
		IS_WINE           = 0x01,
		IS_WOW64          = 0x02,
		OS_XP_PLUS        = 0x04,
		OS_VISTA_PLUS     = 0x08,
		OS_WINDOWS7_PLUS  = 0x10,
		OS_WINDOWS8_PLUS  = 0x20,
		OS_WINDOWS10_PLUS = 0x40,
		OS_WINDOWS11_PLUS = 0x80
	};

	inline bool isWine() { return (flags & IS_WINE) != 0; }
	inline bool isWow64() { return (flags & IS_WOW64) != 0; }
	inline bool isOsXpPlus() { return (flags & OS_XP_PLUS) != 0; }
	inline bool isOsVistaPlus() { return (flags & OS_VISTA_PLUS) != 0; }
	inline bool isOsWin7Plus() { return (flags & OS_WINDOWS7_PLUS) != 0; }
	inline bool isOsWin8Plus() { return (flags & OS_WINDOWS8_PLUS) != 0; }
	inline bool isOsWin10Plus() { return (flags & OS_WINDOWS10_PLUS) != 0; }
	inline bool isOsWin11Plus() { return (flags & OS_WINDOWS11_PLUS) != 0; }
	void setWine(bool flag);

	bool isWow64Process();
	bool detectWine();
	inline unsigned getCPUCount() { return sysInfo.dwNumberOfProcessors; }
	const char* getProcArchString();
#else
#endif

	string getFormattedOsName();
	string getFormattedOsVerNum();
}

#endif // SYS_VERSION_H_

#ifndef WIN_CONF_H_
#define WIN_CONF_H_

#ifdef _WIN64
#undef OSVER_WIN_XP
#undef OSVER_WIN_VISTA
#else
#define OSVER_WIN_XP
#endif

// https://msdn.microsoft.com/ru-ru/library/windows/desktop/aa383745%28v=vs.85%29.aspx
#if defined(_M_ARM) || defined (_M_ARM64)
# define _WIN32_WINNT _WIN32_WINNT_WINBLUE
#else
#ifndef _WIN32_WINNT
# ifdef OSVER_WIN_XP
#  define _WIN32_WINNT _WIN32_WINNT_WINXP
# elif defined(OSVER_WIN_VISTA)
#  define _WIN32_WINNT _WIN32_WINNT_VISTA
# else // Win7+
#  define _WIN32_WINNT _WIN32_WINNT_WIN7
# endif
#endif
#endif // _WIN32_WINNT

#ifndef _WIN32_IE
# ifdef OSVER_WIN_XP
#  define _WIN32_IE _WIN32_IE_IE80
# endif
#endif // _WIN32_IE

#ifndef _RICHEDIT_VER
#define _RICHEDIT_VER 0x0500
#endif

#endif // WIN_CONF_H_

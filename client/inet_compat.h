#ifndef INET_COMPAT_H_
#define INET_COMPAT_H_

#ifdef _WIN32
#include "w.h"
#endif

#ifdef OSVER_WIN_XP
int inet_pton_compat(int af, const char* src, void* dst);
const char* inet_ntop_compat(int af, const void* src, char* dst, int size);
#else
#define inet_pton_compat inet_pton
#define inet_ntop_compat inet_ntop
#endif

#endif // INET_COMPAT_H_

#ifndef DCPLUSPLUS_MAKE_DEFS_STDAFX_H
#define DCPLUSPLUS_MAKE_DEFS_STDAFX_H

#define BOOST_ALL_NO_LIB 1

#include "../client/stdinc.h"

#if defined(_DEBUG) && defined(_WIN32)
#include <crtdbg.h>
#endif

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <sys/types.h>

#include <string>
#include <vector>

#endif // !defined(DCPLUSPLUS_MAKE_DEFS_STDAFX_H)

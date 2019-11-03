//////////////////////////////////////////////////////////////////////////////////////
//
// Written by Zoltan Csizmadia, zoltan_csizmadia@yahoo.com
// For companies(Austin,TX): If you would like to get my resume, send an email.
//
// The source is free, but if you want to use it, mention my name and e-mail address
//
// History:
//    1.0      Initial version                  Zoltan Csizmadia
//
//////////////////////////////////////////////////////////////////////////////////////
//
// ExtendedTrace.h
//

#ifndef EXTENDEDTRACE_H_INCLUDED
#define EXTENDEDTRACE_H_INCLUDED

#include "../client/w.h"

#pragma comment(lib, "dbghelp.lib")

class File;

BOOL InitSymInfo(PCSTR);
BOOL UninitSymInfo();

#ifndef _WIN64
void StackTrace(HANDLE hThread, File& f, const PCONTEXT pCtx);
#else
void StackTrace(HANDLE hThread, File& f, const PCONTEXT pCtx);
#endif

#endif

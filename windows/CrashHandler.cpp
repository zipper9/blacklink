#include <stdafx.h>
#include "ExtendedTrace.h"
#include "../client/CFlyThread.h"
#include "../client/File.h"
#include "../client/Util.h"
#include "MainFrm.h"
#include "WinUtil.h"

static CriticalSection exceptionCS;

static const int DEBUG_BUFSIZE = 8192;
static int recursion = 0;
static char exeTTH[192*8/(5*8)+2];
static bool firstException = true;

static char debugBuf[DEBUG_BUFSIZE];

static tstring crashMessageTitle(_T(APPNAME " has crashed"));

static string getExceptionName(DWORD code)
{
	switch(code)
	{ 
		case EXCEPTION_ACCESS_VIOLATION:
			return "Access violation"; break; 
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			return "Array out of range"; break; 
		case EXCEPTION_BREAKPOINT:
			return "Breakpoint"; break; 
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			return "Read or write error"; break; 
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			return "Floating-point error"; break; 
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			return "Floating-point division by zero"; break; 
		case EXCEPTION_FLT_INEXACT_RESULT:
			return "Floating-point inexact result"; break; 
		case EXCEPTION_FLT_INVALID_OPERATION:
			return "Unknown floating-point error"; break; 
		case EXCEPTION_FLT_OVERFLOW:
			return "Floating-point overflow"; break; 
		case EXCEPTION_FLT_STACK_CHECK:
			return "Floating-point operation caused stack overflow"; break; 
		case EXCEPTION_FLT_UNDERFLOW:
			return "Floating-point underflow"; break; 
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			return "Illegal instruction"; break; 
		case EXCEPTION_IN_PAGE_ERROR:
			return "Page error"; break; 
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			return "Integer division by zero"; break; 
		case EXCEPTION_INT_OVERFLOW:
			return "Integer overflow"; break; 
		case EXCEPTION_INVALID_DISPOSITION:
			return "Invalid disposition"; break; 
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			return "Noncontinueable exception"; break; 
		case EXCEPTION_PRIV_INSTRUCTION:
			return "Invalid instruction"; break; 
		case EXCEPTION_SINGLE_STEP:
			return "Single step executed"; break; 
		case EXCEPTION_STACK_OVERFLOW:
			return "Stack overflow"; break; 
	}
	return "Exception code 0x" + Util::toHexString(code);
}

#define LIT(n) n, sizeof(n)-1

static LONG handleCrash(unsigned long code, const string& error, PCONTEXT context)
{
	CFlyLock(exceptionCS);

	if (recursion++ > 30)
		ExitProcess((UINT) -1);

	tstring pdbFileName = Util::getModuleFileName();
	if (Text::isAsciiSuffix2(pdbFileName, tstring(_T("exe"))))
	{
		size_t pos = pdbFileName.length()-3;
		pdbFileName[pos]   = _T('p');
		pdbFileName[pos+1] = _T('d');
		pdbFileName[pos+2] = _T('b');
	}

	tstring symbolsPath = pdbFileName;
	tstring::size_type pos = symbolsPath.rfind(PATH_SEPARATOR);
	if (pos != tstring::npos) symbolsPath.erase(pos);
	string symbolsPathAcp;
	Text::wideToAcp(symbolsPath, symbolsPathAcp);
	InitSymInfo(symbolsPathAcp.c_str());

	if (File::getSize(pdbFileName) <= 0)
	{
		::MessageBox(WinUtil::g_mainWnd,
			_T(APPNAME " has crashed and you don't have " APPNAME ".pdb file installed."),
			crashMessageTitle.c_str(), MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}

#if 0
	if ((!SETTING(SOUND_EXC).empty()) && (!SETTING(SOUNDS_DISABLED)))
		WinUtil::playSound(Text::toT(SETTING(SOUND_EXC)));
#endif

	if (MainFrame::getMainFrame())
	{
		NOTIFYICONDATA m_nid;
		m_nid.cbSize = sizeof(NOTIFYICONDATA);
		m_nid.hWnd = WinUtil::g_mainWnd;
		m_nid.uID = 0;
		m_nid.uFlags = NIF_INFO;
		m_nid.uTimeout = 5000;
		m_nid.dwInfoFlags = NIIF_WARNING;
		_tcscpy(m_nid.szInfo, _T("exceptioninfo.txt was generated"));
		_tcscpy(m_nid.szInfoTitle, crashMessageTitle.c_str());
		Shell_NotifyIcon(NIM_MODIFY, &m_nid);
	}

	auto exceptionFilePath = Util::getConfigPath() + "exceptioninfo.txt";

#if 0 // Don't delete the file!
	if (firstException)
	{
		File::deleteFile(exceptionFilePath);
		firstException = false;
	}
#endif

	try
	{
		File f(exceptionFilePath, File::WRITE, File::OPEN | File::CREATE);
		f.setEndPos(0);

		sprintf(debugBuf, "Code: %x (%s)\r\nVersion: " A_VERSIONSTRING "\r\n", code, error.c_str());

		f.write(debugBuf, strlen(debugBuf));

		OSVERSIONINFOEX ver;
		memset(&ver, 0, sizeof(OSVERSIONINFOEX));
		ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		if (!GetVersionEx((OSVERSIONINFO*)&ver))
		{
			ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			GetVersionEx((OSVERSIONINFO*)&ver);
		}

		sprintf(debugBuf, "Major: %d\r\nMinor: %d\r\nBuild: %d\r\nSP: %d\r\nType: %d\r\n",
			ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber,
			ver.wServicePackMajor, ver.wProductType);

		f.write(debugBuf, strlen(debugBuf));
		time_t now;
		time(&now);
		strftime(debugBuf, DEBUG_BUFSIZE, "Time: %Y-%m-%d %H:%M:%S\r\n", localtime(&now));

		f.write(debugBuf, strlen(debugBuf));

		f.write(LIT("\r\n"));

		StackTrace(GetCurrentThread(), f, context);

		f.write(LIT("\r\n"));
	}
	catch (const FileException& e)
	{
		auto msg = "Crash details could not be written to " + exceptionFilePath + " (" + e.what() + "). Ensure that the directory is writable.";
		::MessageBox(WinUtil::g_mainWnd, Text::toT(msg).c_str(), crashMessageTitle.c_str(), MB_OK | MB_ICONERROR);
	}

	auto msg = APPNAME " just encountered a fatal bug and details have been written to " + exceptionFilePath;
	::MessageBox(WinUtil::g_mainWnd, Text::toT(msg).c_str(), crashMessageTitle.c_str(), MB_OK | MB_ICONERROR);

	UninitSymInfo();
	ExitProcess((UINT) -1);
}

LONG __stdcall DCUnhandledExceptionFilter(LPEXCEPTION_POINTERS e)
{
	auto code = e->ExceptionRecord->ExceptionCode;
	return handleCrash(code, getExceptionName(code), e->ContextRecord);
}

#ifndef COMMAND_LINE_H_
#define COMMAND_LINE_H_

#include "../client/compiler.h"
#include "../client/typedefs.h"

struct ParsedCommandLine
{
	bool sqliteDisableJournal = false;
	bool sqliteSyncOff = false;
	bool sqliteTrace = false;
	bool disableSplash = false;
	bool disableGDIPlus = false;
	bool disablePortTest = false;
	bool multipleInstances = false;
	bool delay = false;
	int setWine = 0;
#ifdef SSA_SHELL_INTEGRATION
	int installShellExt = 0;
#endif
	int installAutoRunShortcut = 0;
	bool addFirewallEx = false;
	tstring openMagnet;
	tstring openHub;
	tstring openFile;	
	tstring shareFolder;
};

bool parseCommandLine(ParsedCommandLine& out, const WCHAR* cmdLine);

#endif // COMMAND_LINE_H_

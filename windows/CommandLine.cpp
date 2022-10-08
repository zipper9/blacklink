#include "stdafx.h"
#include "CommandLine.h"
#include "Util.h"

bool parseCommandLine(ParsedCommandLine& out, const WCHAR* cmdLine)
{
	if (!cmdLine) cmdLine = GetCommandLineW();
	int count;
	LPWSTR* result = CommandLineToArgvW(cmdLine, &count);
	if (!result) return false;
	for (int i = 0; i < count; ++i)
	{
		if (!wcscmp(result[i], L"/sqlite_synchronous_off"))
		{
			out.sqliteSyncOff = true;
			continue;
		}
		if (!wcscmp(result[i], L"/sqltrace"))
		{
			out.sqliteTrace = true;
			continue;
		}
		if (!wcscmp(result[i], L"/nologo"))
		{
			out.disableSplash = true;
			continue;
		}
		if (!wcscmp(result[i], L"/nogdiplus"))
		{
			out.disableGDIPlus = true;
			continue;
		}
		if (!wcscmp(result[i], L"/notestport"))
		{
			out.disablePortTest = true;
			continue;
		}
		if (!wcscmp(result[i], L"/q"))
		{
			out.multipleInstances = true;
			continue;
		}
		if (!wcscmp(result[i], L"/c"))
		{
			out.multipleInstances = out.delay = true;
			continue;
		}
		if (!wcscmp(result[i], L"/nowine"))
		{
			out.setWine = -1;
			continue;
		}
		if (!wcscmp(result[i], L"/forcewine"))
		{
			out.setWine = 1;
			continue;
		}
#ifdef SSA_SHELL_INTEGRATION
		if (!wcscmp(result[i], L"/installShellExt"))
		{
			out.installShellExt = 1;
			continue;
		}
		if (!wcscmp(result[i], L"/uninstallShellExt"))
		{
			out.installShellExt = -1;
			continue;
		}
#endif
		if (!wcscmp(result[i], L"/installStartup"))
		{
			out.installAutoRunShortcut = 1;
			continue;
		}
		if (!wcscmp(result[i], L"/uninstallStartup"))
		{
			out.installAutoRunShortcut = -1;
			continue;
		}
		if (!wcscmp(result[i], L"/open"))
		{
			if (++i == count) break;
			out.openFile = result[i];
			continue;
		}
		if (!wcscmp(result[i], L"/magnet"))
		{
			if (++i == count) break;
			out.openMagnet = result[i];
			continue;
		}
		if (!wcscmp(result[i], L"/share"))
		{
			if (++i == count) break;
			out.shareFolder = result[i];
			continue;
		}
		if (!wcscmp(result[i], L"/addFw"))
		{
			out.addFirewallEx = true;
			continue;
		}
		if (Util::isMagnetLink(result[i]))
		{
			out.openMagnet = result[i];
			continue;
		}
		Util::ParsedUrl url;
		Util::decodeUrl(Text::fromT(result[i]), url);
		if (Util::getHubProtocol(url.protocol) && !url.host.empty() && url.port != 0)
			out.openHub = result[i];
	}
	LocalFree(result);
	return true;
}

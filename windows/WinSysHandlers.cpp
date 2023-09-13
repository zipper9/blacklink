#include "stdafx.h"
#include "WinSysHandlers.h"
#include "LogManager.h"
#include "../client/ResourceManager.h"
#include "../client/BaseUtil.h"
#include "../client/Text.h"
#include "../client/AppPaths.h"
#include "RegKey.h"

bool WinUtil::hubUrlHandlersRegistered = false;
bool WinUtil::magnetHandlerRegistered = false;
bool WinUtil::dclstHandlerRegistered = false;

static bool registerUrlHandler(const TCHAR* proto, const TCHAR* description)
{
	WinUtil::RegKey key;
	tstring value;
	tstring exePath = Util::getModuleFileName();
	tstring app = _T('\"') + exePath + _T("\" /magnet \"%1\"");
	tstring pathProto = _T("SOFTWARE\\Classes\\");
	pathProto += proto;
	tstring pathCommand = pathProto + _T("\\Shell\\Open\\Command");

	if (key.open(HKEY_CURRENT_USER, pathCommand.c_str(), KEY_READ))
		key.readString(nullptr, value);

	if (stricmp(app, value) == 0)
		return true;

	if (!key.create(HKEY_CURRENT_USER, pathProto.c_str(), KEY_WRITE))
		return false;

	bool result = false;
	do
	{
		if (!key.writeString(nullptr, description, _tcslen(description))) break;
		if (!key.writeString(_T("URL Protocol"), Util::emptyStringT)) break;

		if (!key.create(HKEY_CURRENT_USER, pathCommand.c_str(), KEY_WRITE)) break;
		if (!key.writeString(nullptr, app)) break;

		tstring pathIcon = pathProto + _T("\\DefaultIcon");
		if (!key.create(HKEY_CURRENT_USER, pathIcon.c_str(), KEY_WRITE)) break;
		if (!key.writeString(nullptr, exePath)) break;
		result = true;
	} while (0);

	return result;
}

void WinUtil::registerHubUrlHandlers()
{
	if (registerUrlHandler(_T("dchub"), _T("URL:Direct Connect Protocol")))
		hubUrlHandlersRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "dchub"));

	if (registerUrlHandler(_T("nmdcs"), _T("URL:Direct Connect Protocol")))
		hubUrlHandlersRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "nmdcs"));

	if (registerUrlHandler(_T("adc"), _T("URL:Advanced Direct Connect Protocol")))
		hubUrlHandlersRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "adc"));

	if (registerUrlHandler(_T("adcs"), _T("URL:Advanced Direct Connect Protocol")))
		hubUrlHandlersRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "adcs"));
}

static void internalDeleteRegistryKey(const tstring& key)
{
	tstring path = _T("SOFTWARE\\Classes\\") + key;
	if (SHDeleteKey(HKEY_CURRENT_USER, path.c_str()) != ERROR_SUCCESS)
		LogManager::message(STRING_F(ERROR_DELETING_REGISTRY_KEY, Text::fromT(path) % Util::translateError()));
}

void WinUtil::unregisterHubUrlHandlers()
{
	internalDeleteRegistryKey(_T("dchub"));
	internalDeleteRegistryKey(_T("nmdcs"));
	internalDeleteRegistryKey(_T("adc"));
	internalDeleteRegistryKey(_T("adcs"));
	hubUrlHandlersRegistered = false;
}

void WinUtil::registerMagnetHandler()
{
	if (registerUrlHandler(_T("magnet"), _T("URL:Magnet Link")))
		magnetHandlerRegistered = true;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "magnet"));
}

void WinUtil::unregisterMagnetHandler()
{
	internalDeleteRegistryKey(_T("magnet"));
	magnetHandlerRegistered = false;
}

static bool registerFileHandler(const TCHAR* names[], const TCHAR* description)
{
	WinUtil::RegKey key;
	tstring value;
	tstring exePath = Util::getModuleFileName();
	tstring app = _T('\"') + exePath + _T("\" /open \"%1\"");
	tstring pathExt = _T("SOFTWARE\\Classes\\");
	pathExt += names[0];
	tstring pathCommand = pathExt + _T("\\Shell\\Open\\Command");

	if (key.open(HKEY_CURRENT_USER, pathCommand.c_str(), KEY_READ))
		key.readString(nullptr, value);

	if (stricmp(app, value) == 0)
		return true;

	if (!key.create(HKEY_CURRENT_USER, pathExt.c_str(), KEY_WRITE))
		return false;

	bool result = false;
	do
	{
		if (!key.writeString(nullptr, description, _tcslen(description))) break;

		if (!key.create(HKEY_CURRENT_USER, pathCommand.c_str(), KEY_WRITE)) break;
		if (!key.writeString(nullptr, app)) break;

		tstring pathIcon = pathExt + _T("\\DefaultIcon");
		if (!key.create(HKEY_CURRENT_USER, pathIcon.c_str(), KEY_WRITE)) break;
		if (!key.writeString(nullptr, exePath)) break;

		int i = 1;
		DWORD len = _tcslen(names[0]);
		while (true)
		{
			if (!names[i])
			{
				result = true;
				break;
			}
			tstring path = _T("SOFTWARE\\Classes\\");
			path += names[i];
			if (!key.create(HKEY_CURRENT_USER, path.c_str(), KEY_WRITE)) break;
			if (!key.writeString(nullptr, names[0], len)) break;
			i++;
		}
	} while (0);

	return result;
}

static const TCHAR* dcLstNames[] = { _T("DcLst download list"), _T(".dclst"), _T(".dcls"), nullptr };

void WinUtil::registerDclstHandler()
{
	if (registerFileHandler(dcLstNames, CTSTRING(DCLST_DESCRIPTION)))
		dclstHandlerRegistered = true;
	else
		LogManager::message(STRING(ERROR_CREATING_REGISTRY_KEY_DCLST));
}

void WinUtil::unregisterDclstHandler()
{
	int i = 0;
	while (dcLstNames[i])
	{
		internalDeleteRegistryKey(dcLstNames[i]);
		i++;
	}
	dclstHandlerRegistered = false;
}

#include "stdafx.h"
#include "WinSysHandlers.h"
#include "../client/SettingsManager.h"
#include "../client/LogManager.h"
#include "../client/ResourceManager.h"
#include "../client/BaseUtil.h"
#include "../client/Text.h"
#include "../client/AppPaths.h"
#include "RegKey.h"
#include "ConfUI.h"

int WinUtil::registeredHandlerMask = 0;

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
		registeredHandlerMask |= REG_HANDLER_HUB_URL;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "dchub"));

	if (registerUrlHandler(_T("nmdcs"), _T("URL:Direct Connect Protocol")))
		registeredHandlerMask |= REG_HANDLER_HUB_URL;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "nmdcs"));

	if (registerUrlHandler(_T("adc"), _T("URL:Advanced Direct Connect Protocol")))
		registeredHandlerMask |= REG_HANDLER_HUB_URL;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "adc"));

	if (registerUrlHandler(_T("adcs"), _T("URL:Advanced Direct Connect Protocol")))
		registeredHandlerMask |= REG_HANDLER_HUB_URL;
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
	registeredHandlerMask &= ~REG_HANDLER_HUB_URL;
}

void WinUtil::registerMagnetHandler()
{
	if (registerUrlHandler(_T("magnet"), _T("URL:Magnet Link")))
		registeredHandlerMask |= REG_HANDLER_MAGNET;
	else
		LogManager::message(STRING_F(ERROR_REGISTERING_PROTOCOL_HANDLER, "magnet"));
}

void WinUtil::unregisterMagnetHandler()
{
	internalDeleteRegistryKey(_T("magnet"));
	registeredHandlerMask &= ~REG_HANDLER_MAGNET;
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
		registeredHandlerMask |= REG_HANDLER_DCLST;
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
	registeredHandlerMask &= ~REG_HANDLER_DCLST;
}

int WinUtil::getRegHandlerSettings()
{
	int result = 0;
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::REGISTER_URL_HANDLER))
		result |= REG_HANDLER_HUB_URL;
	if (ss->getBool(Conf::REGISTER_MAGNET_HANDLER))
		result |= REG_HANDLER_MAGNET;
	if (ss->getBool(Conf::REGISTER_DCLST_HANDLER))
		result |= REG_HANDLER_DCLST;
	return result;
}

void WinUtil::applyRegHandlerSettings(int settings, int mask)
{
	if (mask & REG_HANDLER_HUB_URL)
	{
		if (settings & REG_HANDLER_HUB_URL)
			registerHubUrlHandlers();
		else if (registeredHandlerMask & REG_HANDLER_HUB_URL)
			unregisterHubUrlHandlers();
	}
	if (mask & REG_HANDLER_MAGNET)
	{
		if (settings & REG_HANDLER_MAGNET)
			registerMagnetHandler();
		else if (registeredHandlerMask & REG_HANDLER_MAGNET)
			unregisterMagnetHandler();
	}
	if (mask & REG_HANDLER_DCLST)
	{
		if (settings & REG_HANDLER_DCLST)
			registerDclstHandler();
		else if (registeredHandlerMask & REG_HANDLER_DCLST)
			unregisterDclstHandler();
	}
}

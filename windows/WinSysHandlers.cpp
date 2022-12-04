#include "stdafx.h"
#include "WinSysHandlers.h"
#include "LogManager.h"
#include "../client/ResourceManager.h"
#include "../client/Util.h"

bool WinUtil::hubUrlHandlersRegistered = false;
bool WinUtil::magnetHandlerRegistered = false;
bool WinUtil::dclstHandlerRegistered = false;

static bool regReadString(HKEY key, const TCHAR* name, tstring& value)
{
	TCHAR buf[512];
	DWORD size = sizeof(buf);
	DWORD type;
	if (RegQueryValueEx(key, name, nullptr, &type, (BYTE *) buf, &size) != ERROR_SUCCESS ||
	    (type != REG_SZ && type != REG_EXPAND_SZ))
	{
		value.clear();
		return false;
	}
	size /= sizeof(TCHAR);
	if (size && buf[size-1] == 0) size--;
	value.assign(buf, size);
	return true;
}

static bool regWriteString(HKEY key, const TCHAR* name, const TCHAR* value, DWORD len)
{
	return RegSetValueEx(key, name, 0, REG_SZ, (const BYTE *) value, (len + 1) * sizeof(TCHAR)) == ERROR_SUCCESS;
}

static inline bool regWriteString(HKEY key, const TCHAR* name, const tstring& str)
{
	return regWriteString(key, name, str.c_str(), str.length());
}

static bool registerUrlHandler(const TCHAR* proto, const TCHAR* description)
{
	HKEY key = nullptr;
	tstring value;
	tstring exePath = Util::getModuleFileName();
	tstring app = _T('\"') + exePath + _T("\" /magnet \"%1\"");
	tstring pathProto = _T("SOFTWARE\\Classes\\");
	pathProto += proto;
	tstring pathCommand = pathProto + _T("\\Shell\\Open\\Command");

	if (RegOpenKeyEx(HKEY_CURRENT_USER, pathCommand.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		regReadString(key, nullptr, value);
		RegCloseKey(key);
	}

	if (stricmp(app, value) == 0)
		return true;

	if (RegCreateKeyEx(HKEY_CURRENT_USER, pathProto.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
		return false;

	bool result = false;
	do
	{
		if (!regWriteString(key, nullptr, description, _tcslen(description))) break;
		if (!regWriteString(key, _T("URL Protocol"), Util::emptyStringT)) break;
		RegCloseKey(key);
		key = nullptr;

		if (RegCreateKeyEx(HKEY_CURRENT_USER, pathCommand.c_str(), 0, nullptr,
		    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
		if (!regWriteString(key, nullptr, app)) break;
		RegCloseKey(key);
		key = nullptr;

		tstring pathIcon = pathProto + _T("\\DefaultIcon");
		if (RegCreateKeyEx(HKEY_CURRENT_USER, pathIcon.c_str(), 0, nullptr,
		    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
		if (!regWriteString(key, nullptr, exePath)) break;
		result = true;
	} while (0);

	if (key) RegCloseKey(key);
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
	HKEY key = nullptr;
	tstring value;
	tstring exePath = Util::getModuleFileName();
	tstring app = _T('\"') + exePath + _T("\" /open \"%1\"");
	tstring pathExt = _T("SOFTWARE\\Classes\\");
	pathExt += names[0];
	tstring pathCommand = pathExt + _T("\\Shell\\Open\\Command");

	if (RegOpenKeyEx(HKEY_CURRENT_USER, pathCommand.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		regReadString(key, nullptr, value);
		RegCloseKey(key);
	}

	if (stricmp(app, value) == 0)
		return true;

	if (RegCreateKeyEx(HKEY_CURRENT_USER, pathExt.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
		return false;

	bool result = false;
	do
	{
		if (!regWriteString(key, nullptr, description, _tcslen(description))) break;
		RegCloseKey(key);
		key = nullptr;

		if (RegCreateKeyEx(HKEY_CURRENT_USER, pathCommand.c_str(), 0, nullptr,
		    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
		if (!regWriteString(key, nullptr, app)) break;
		RegCloseKey(key);
		key = nullptr;

		tstring pathIcon = pathExt + _T("\\DefaultIcon");
		if (RegCreateKeyEx(HKEY_CURRENT_USER, pathIcon.c_str(), 0, nullptr,
		    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
		if (!regWriteString(key, nullptr, exePath)) break;
		RegCloseKey(key);
		key = nullptr;

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
			if (RegCreateKeyEx(HKEY_CURRENT_USER, path.c_str(), 0, nullptr,
			    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) break;
			if (!regWriteString(key, nullptr, names[0], len)) break;
			RegCloseKey(key);
			key = nullptr;
			i++;
		}
	} while (0);

	if (key) RegCloseKey(key);
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

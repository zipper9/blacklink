#include "stdafx.h"
#include "RegKey.h"

void WinUtil::RegKey::close() noexcept
{
	if (hKey)
	{
		RegCloseKey(hKey);
		hKey = nullptr;
	}
}

bool WinUtil::RegKey::open(HKEY parent, const TCHAR* name, REGSAM access) noexcept
{
	HKEY key;
	if (RegOpenKeyEx(parent, name, 0, access, &key) != ERROR_SUCCESS) return false;
	attach(key);
	return true;
}

bool WinUtil::RegKey::create(HKEY parent, const TCHAR* name, REGSAM access) noexcept
{
	HKEY key;
	if (RegCreateKeyEx(parent, name, 0, nullptr, 0, access, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
	attach(key);
	return true;
}

bool WinUtil::expandEnvStrings(tstring& s)
{
	unsigned bufSize = ExpandEnvironmentStrings(s.c_str(), nullptr, 0);
	if (!bufSize) return false;
	tstring ex;
	ex.resize(bufSize);
	unsigned result = ExpandEnvironmentStrings(s.c_str(), &ex[0], bufSize);
	if (!result || result > bufSize) return false;
	ex.resize(result-1);
	s = std::move(ex);
	return true;
}

bool WinUtil::RegKey::readString(const TCHAR* name, tstring& value, bool expandEnv) noexcept
{
	DWORD size = 0, type;
	int result = RegQueryValueEx(hKey, name, nullptr, &type, nullptr, &size);
	if ((result != ERROR_SUCCESS && result != ERROR_MORE_DATA) ||
	    (type != REG_SZ && type != REG_EXPAND_SZ))
	{
		value.clear();
		return false;
	}
	value.resize((size + 1) / 2);
	TCHAR* buf = &value[0];
	if (RegQueryValueEx(hKey, name, nullptr, &type, (BYTE *) buf, &size) != ERROR_SUCCESS ||
	    (type != REG_SZ && type != REG_EXPAND_SZ))
	{
		value.clear();
		return false;
	}
	size /= sizeof(TCHAR);
	if (size && buf[size-1] == 0) size--;
	value.resize(size);
	if (expandEnv && type == REG_EXPAND_SZ && value.find('%') != tstring::npos)
		return expandEnvStrings(value);
	return true;
}

bool WinUtil::RegKey::writeString(const TCHAR* name, const TCHAR* value, size_t len) noexcept
{
	return RegSetValueEx(hKey, name, 0, REG_SZ, (const BYTE *) value, (len + 1) * sizeof(TCHAR)) == ERROR_SUCCESS;
}

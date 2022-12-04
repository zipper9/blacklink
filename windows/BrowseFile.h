#ifndef BROWSE_FILE_H_
#define BROWSE_FILE_H_

#include "../client/w.h"
#include "../client/BaseUtil.h"

namespace WinUtil
{
	bool browseFile(tstring& target, HWND owner = nullptr, bool save = true, const tstring& initialDir = Util::emptyStringT, const TCHAR* types = nullptr, const TCHAR* defExt = nullptr, const GUID* id = nullptr);
	bool browseDirectory(tstring& target, HWND owner = nullptr, const GUID* id = nullptr);
}

#endif // BROWSE_FILE_H_

#include "stdafx.h"
#include "BrowseFile.h"
#include "../client/PathUtil.h"

#ifdef OSVER_WIN_XP
#include "../client/Util.h"
#include "../client/SysVersion.h"

static int CALLBACK browseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lp*/, LPARAM pData)
{
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
			break;
	}
	return 0;
}

static bool browseDirectoryLegacy(tstring& target, HWND owner)
{
	TCHAR buf[MAX_PATH];
	BROWSEINFO bi = {0};
	bi.hwndOwner = owner;
	bi.pszDisplayName = buf;
	bi.lpszTitle = CTSTRING(CHOOSE_FOLDER);
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
	bi.lParam = (LPARAM)target.c_str();
	bi.lpfn = &browseCallbackProc;
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (!pidl) return false;
	bool result = false;
	if (SHGetPathFromIDList(pidl, buf))
	{
		target = buf;
		Util::appendPathSeparator(target);
		result = true;
	}
	CoTaskMemFree(pidl);
	return result;
}

static bool browseFileLegacy(tstring& target, HWND owner, bool save, const tstring& initialDir, const TCHAR* types, const TCHAR* defExt)
{
	OPENFILENAME ofn = { 0 }; // common dialog box structure
	target = Text::toT(Util::validateFileName(Text::fromT(target)));
	unique_ptr<TCHAR[]> buf(new TCHAR[FULL_MAX_PATH]);
	size_t len = min<size_t>(target.length(), FULL_MAX_PATH-1);
	memcpy(buf.get(), target.data(), len*sizeof(TCHAR));
	buf[len] = 0;
	// Initialize OPENFILENAME
	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner = owner;
	ofn.lpstrFile = buf.get();
	ofn.lpstrFilter = types;
	ofn.lpstrDefExt = defExt;
	ofn.nFilterIndex = 1;

	if (!initialDir.empty())
	{
		ofn.lpstrInitialDir = initialDir.c_str();
	}
	ofn.nMaxFile = FULL_MAX_PATH;
	ofn.Flags = (save ? 0 : OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);

	// Display the Open dialog box.
	if ((save ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn)) != FALSE)
	{
		target = ofn.lpstrFile;
		return true;
	}
	else
	{
		dcdebug("Error WinUtil::browseFile CommDlgExtendedError() = %x\n", CommDlgExtendedError());
	}
	return false;
}

#endif

static bool addFileDialogOptions(IFileOpenDialog* fileOpen, FILEOPENDIALOGOPTIONS options)
{
	FILEOPENDIALOGOPTIONS fos;
	if (FAILED(fileOpen->GetOptions(&fos))) return false;
	fos |= options;
	return SUCCEEDED(fileOpen->SetOptions(fos));
}

static IShellItem* createItemFromParsingName(const wstring& path)
{
	IShellItem* shellItem = nullptr;
#ifdef OSVER_WIN_XP
	typedef HRESULT (STDAPICALLTYPE *fnCreateItemFromParsingName)(const WCHAR*, IBindCtx*, REFIID, void**);
	static fnCreateItemFromParsingName pCreateItemFromParsingName = nullptr;
	static bool resolved = false;
	if (!resolved)
	{
		HMODULE shell32lib = LoadLibrary(_T("shell32.dll"));
		if (shell32lib)
			pCreateItemFromParsingName = (fnCreateItemFromParsingName) GetProcAddress(shell32lib, "SHCreateItemFromParsingName");
		resolved = true;
	}
	if (pCreateItemFromParsingName)
		pCreateItemFromParsingName(path.c_str(), nullptr, IID_IShellItem, (void **) &shellItem);
#else
	SHCreateItemFromParsingName(path.c_str(), nullptr, IID_IShellItem, (void **) &shellItem);
#endif
	return shellItem;
}

static bool browseDirectoryNew(tstring& target, HWND owner, const GUID* id)
{
	bool result = false;
	IFileOpenDialog *pFileOpen = nullptr;

	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
		IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

	if (SUCCEEDED(hr))
	{
		if (id) pFileOpen->SetClientGuid(*id);
		if (addFileDialogOptions(pFileOpen, FOS_PICKFOLDERS | FOS_NOCHANGEDIR))
		{
			if (!target.empty())
			{
				IShellItem* shellItem = createItemFromParsingName(target);
				if (shellItem)
				{
					pFileOpen->SetFolder(shellItem);
					shellItem->Release();
				}
			}
			pFileOpen->SetTitle(CWSTRING(CHOOSE_FOLDER));
			hr = pFileOpen->Show(owner);
			if (SUCCEEDED(hr))
			{
				IShellItem *pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
					if (SUCCEEDED(hr))
					{
						target = pszFilePath;
						Util::appendPathSeparator(target);
						CoTaskMemFree(pszFilePath);
						result = true;
					}
					pItem->Release();
				}
			}
		}
	}
	if (pFileOpen) pFileOpen->Release();
	return result;
}

static void setFolder(IFileOpenDialog* fileOpen, const wstring& path)
{
	IShellItem* shellItem = createItemFromParsingName(path);
	if (shellItem)
	{
		fileOpen->SetFolder(shellItem);
		shellItem->Release();
	}
}

static bool browseFileNew(tstring& target, HWND owner, bool save, const tstring& initialDir, const TCHAR* types, const TCHAR* defExt, const GUID* id)
{
	bool result = false;
	IFileOpenDialog *pFileOpen = nullptr;

	HRESULT hr = CoCreateInstance(
		save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
		save ? IID_IFileSaveDialog : IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

	if (SUCCEEDED(hr))
	{
		if (id) pFileOpen->SetClientGuid(*id);
		if (addFileDialogOptions(pFileOpen, FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR/* | FOS_DONTADDTORECENT*/))
		{
			if (types)
			{
				vector<COMDLG_FILTERSPEC> filters;
				while (true)
				{
					const WCHAR* name = types;
					if (!*name) break;
					const WCHAR* next = wcschr(name, 0);
					const WCHAR* spec = next + 1;
					filters.emplace_back(COMDLG_FILTERSPEC{name, spec});
					next = wcschr(spec, 0);
					types = next + 1;
				}
				pFileOpen->SetFileTypes(filters.size(), filters.data());
			}
			if (defExt) pFileOpen->SetDefaultExtension(defExt);
			if (!initialDir.empty())
				setFolder(pFileOpen, initialDir);
			if (!target.empty())
			{
				auto pos = target.rfind(PATH_SEPARATOR);
				if (pos != tstring::npos && initialDir.empty())
					setFolder(pFileOpen, target.substr(0, pos));
				if (pos != tstring::npos)
					target.erase(0, pos + 1);
				pFileOpen->SetFileName(target.c_str());
				if (!defExt)
				{
					tstring ext = Util::getFileExtWithoutDot(target);
					if (!ext.empty()) pFileOpen->SetDefaultExtension(ext.c_str());
				}
			}
			hr = pFileOpen->Show(owner);
			if (SUCCEEDED(hr))
			{
				IShellItem *pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
					if (SUCCEEDED(hr))
					{
						target = pszFilePath;
						CoTaskMemFree(pszFilePath);
						result = true;
					}
					pItem->Release();
				}
			}
		}
	}
	if (pFileOpen) pFileOpen->Release();
	return result;
}

bool WinUtil::browseFile(tstring& target, HWND owner, bool save, const tstring& initialDir, const TCHAR* types, const TCHAR* defExt, const GUID* id)
{
#ifdef OSVER_WIN_XP
	if (!SysVersion::isOsVistaPlus())
		return browseFileLegacy(target, owner, save, initialDir, types, defExt);
#endif
	return browseFileNew(target, owner, save, initialDir, types, defExt, id);
}

bool WinUtil::browseDirectory(tstring& target, HWND owner, const GUID* id)
{
#ifdef OSVER_WIN_XP
	if (!SysVersion::isOsVistaPlus())
		return browseDirectoryLegacy(target, owner);
#endif
	return browseDirectoryNew(target, owner, id);
}

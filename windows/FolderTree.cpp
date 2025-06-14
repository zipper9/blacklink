/* POSSUM_MOD_BEGIN

Most of this code was stolen/adapted/massacred from...

Module : FileTreeCtrl.cpp
Purpose: Interface for an MFC class which provides a tree control similiar
         to the left hand side of explorer

Copyright (c) 1999 - 2003 by PJ Naughter.  (Web: www.naughter.com, Email: pjna@naughter.com)
*/

#include "stdafx.h"
#include "FolderTree.h"
#include "../client/PathUtil.h"
#include "../client/Util.h"
#include "../client/ShareManager.h"
#include "../client/CompatibilityManager.h"
#include "LineDlg.h"
#include "WinUtil.h"
#include "LockRedraw.h"

//Pull in the WNet Lib automatically
#pragma comment(lib, "mpr.lib")

static bool validatePath(const tstring& path)
{
	if (path.empty())
		return false;

	if (path.substr(1, 2) == _T(":\\") || path.substr(0, 2) == _T("\\\\"))
	{
		if (GetFileAttributes(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY)
			return true;
	}

	return false;
}

FolderTreeItemInfo::FolderTreeItemInfo(const FolderTreeItemInfo& src)
{
	m_sFQPath       = src.m_sFQPath;
	m_sRelativePath = src.m_sRelativePath;
	m_bNetworkNode  = src.m_bNetworkNode;
	if (src.m_pNetResource)
	{
		m_pNetResource = new NETRESOURCE;
		memcpy(m_pNetResource, src.m_pNetResource, sizeof(NETRESOURCE));
		
		//Duplicate the strings which are stored in NETRESOURCE as pointers
		if (src.m_pNetResource->lpLocalName)
			m_pNetResource->lpLocalName = _tcsdup(src.m_pNetResource->lpLocalName);
		if (src.m_pNetResource->lpRemoteName)
			m_pNetResource->lpRemoteName = _tcsdup(src.m_pNetResource->lpRemoteName);
		if (src.m_pNetResource->lpComment)
			m_pNetResource->lpComment = _tcsdup(src.m_pNetResource->lpComment);
		if (src.m_pNetResource->lpProvider)
			m_pNetResource->lpProvider = _tcsdup(src.m_pNetResource->lpProvider);
	}
	else
		m_pNetResource = nullptr;
}

FolderTreeItemInfo::~FolderTreeItemInfo()
{
	if (m_pNetResource)
	{
		free(m_pNetResource->lpLocalName);
		free(m_pNetResource->lpRemoteName);
		free(m_pNetResource->lpComment);
		free(m_pNetResource->lpProvider);
		delete m_pNetResource;
	}
}

SystemImageList::SystemImageList()
{
	//Get the temp directory. This is used to then bring back the system image list
	TCHAR pszTempDir[_MAX_PATH + 1];
	pszTempDir[0] = 0;
	GetTempPath(_MAX_PATH + 1, pszTempDir); // TODO - Util::getTempPath()
	TCHAR pszDrive[_MAX_DRIVE + 1];
	pszDrive[0] = 0;
	_tsplitpath(pszTempDir, pszDrive, NULL, NULL, NULL);
	const int nLen = _tcslen(pszDrive);
	if (nLen >= 1)
	{
		if (pszDrive[nLen - 1] != _T('\\'))
			_tcscat(pszDrive, _T("\\"));
			
		//Attach to the system image list
		SHFILEINFO sfi = { 0 };
		HIMAGELIST hSystemImageList = (HIMAGELIST)SHGetFileInfo(pszTempDir, 0, &sfi, sizeof(SHFILEINFO),
		                                                        SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
		m_ImageList.Attach(hSystemImageList);
	}
}

SystemImageList* SystemImageList::getInstance()
{
	static SystemImageList g_instance;
	return &g_instance;
}

SystemImageList::~SystemImageList()
{
	//Detach from the image list to prevent problems on 95/98 where
	//the system image list is shared across processes
	m_ImageList.Detach();
}

ShareEnumerator::ShareEnumerator()
{
	//Set out member variables to defaults
	m_pNTShareEnum = nullptr;
	m_pNTBufferFree = nullptr;
	m_pNTShareInfo = nullptr;
	m_dwShares = 0;

	if (lib.open(_T("NETAPI32.dll")))
	{
		//Get the required function pointers
		m_pNTShareEnum = (NT_NETSHAREENUM*) lib.resolve("NetShareEnum");
		m_pNTBufferFree = (NT_NETAPIBUFFERFREE*) lib.resolve("NetApiBufferFree");
	}

	//Update the array of shares we know about
	Refresh();
}

ShareEnumerator::~ShareEnumerator()
{
	if (m_pNTShareInfo)
		m_pNTBufferFree(m_pNTShareInfo);
}

void ShareEnumerator::Refresh()
{
	m_dwShares = 0;
	//Free the buffer if valid
	if (m_pNTShareInfo)
		m_pNTBufferFree(m_pNTShareInfo);
		
	//Call the function to enumerate the shares
	if (m_pNTShareEnum)
	{
		DWORD dwEntriesRead = 0;
		m_pNTShareEnum(NULL, 502, (LPBYTE*) &m_pNTShareInfo, MAX_PREFERRED_LENGTH, &dwEntriesRead, &m_dwShares, NULL);
	}
}

bool ShareEnumerator::IsShared(const tstring& path) const
{
	//Assume the item is not shared
	bool bShared = false;
	
	if (m_pNTShareInfo)
	{
		for (DWORD i = 0; i < m_dwShares && !bShared; i++)
		{
			bShared = false;
			if (m_pNTShareInfo[i].shi502_type == STYPE_DISKTREE || m_pNTShareInfo[i].shi502_type == STYPE_PRINTQ)
			{
				bShared = stricmp(path.c_str(), m_pNTShareInfo[i].shi502_path) == 0;
			}
#ifdef _DEBUG
			static int g_count = 0;
			const string l_shi502_path = Text::fromT(tstring(m_pNTShareInfo[i].shi502_path));
			dcdebug("ShareEnumerator::IsShared count = %d m_pNTShareInfo[%d].shi502_path = %s path = %s [bShared = %d]\n",
			        ++g_count,
			        i,
			        l_shi502_path.c_str(),
			        Text::fromT(path).c_str(),
			        int(bShared)
			       );
#endif
		}
	}
	return bShared;
}

FolderTree::FolderTree()
{
	m_dwFileHideFlags = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
	                    FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_TEMPORARY;
	m_bShowCompressedUsingDifferentColor = true;
	m_rgbCompressed = RGB(0, 0, 255);
	m_bShowEncryptedUsingDifferentColor = true;
	m_rgbEncrypted = RGB(255, 0, 0);
	m_dwDriveHideFlags = 0;
	m_hNetworkRoot = NULL;
	m_bShowSharedUsingDifferentIcon = true;
	for (size_t i = 0; i < _countof(m_dwMediaID); i++)
		m_dwMediaID[i] = 0xFFFFFFFF;
	m_pShellFolder = nullptr;
	SHGetDesktopFolder(&m_pShellFolder);
	m_bDisplayNetwork = true;
	m_dwNetworkItemTypes = RESOURCETYPE_ANY;
	m_hMyComputerRoot = NULL;
	m_bShowMyComputer = true;
	m_bShowRootedFolder = false;
	m_hRootedFolder = NULL;
	m_bShowDriveLabels = true;
	listener = nullptr;
}

FolderTree::~FolderTree()
{
	if (m_pShellFolder) m_pShellFolder->Release();
}

void FolderTree::PopulateTree()
{
	//attach the image list to the tree control
	SetImageList(*SystemImageList::getInstance()->getImageList(), TVSIL_NORMAL);
	
	//Force a refresh
	Refresh();

	Expand(m_hMyComputerRoot, TVE_EXPAND);
}

void FolderTree::Refresh()
{
	//Just in case this will take some time
	//CWaitCursor waitCursor;
	
	CLockRedraw<> lockRedraw(m_hWnd);
	
	//Get the item which is currently selected
	HTREEITEM hSelItem = GetSelectedItem();
	tstring sItem;
	bool bExpanded = false;
	if (hSelItem)
	{
		sItem = ItemToPath(hSelItem);
		bExpanded = IsExpanded(hSelItem);
	}
	
	theSharedEnumerator.Refresh();
	
	//Remove all nodes that currently exist
	Clear();
	
	//Display the folder items in the tree
	if (m_sRootFolder.empty())
	{
		//Should we insert a "My Computer" node
		if (m_bShowMyComputer)
		{
			FolderTreeItemInfo* pItem = new FolderTreeItemInfo;
			pItem->m_bNetworkNode = false;
			int nIcon = 0;
			int nSelIcon = 0;
			
			//Get the localized name and correct icons for "My Computer"
			LPITEMIDLIST pidl = nullptr;
			if (SUCCEEDED(SHGetFolderLocation(NULL, CSIDL_DRIVES, NULL, 0, &pidl)))
			{
				SHFILEINFO sfi = {0};
				if (SHGetFileInfo((LPCTSTR) pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_DISPLAYNAME))
					pItem->m_sRelativePath = sfi.szDisplayName;
				nIcon = GetIconIndex(pidl);
				nSelIcon = GetSelIconIndex(pidl);
				CoTaskMemFree(pidl);
			}
			
			//Add it to the tree control
			m_hMyComputerRoot = InsertFileItem(TVI_ROOT, pItem, false, nIcon, nSelIcon, false);
			SetHasSharedChildren(m_hMyComputerRoot);
		}
		
		//Display all the drives
		if (!m_bShowMyComputer)
			DisplayDrives(TVI_ROOT, false);
			
		//Also add network neighborhood if requested to do so
		if (m_bDisplayNetwork)
		{
			FolderTreeItemInfo* pItem = new FolderTreeItemInfo;
			pItem->m_bNetworkNode = true;
			int nIcon = 0;
			int nSelIcon = 0;
			
			//Get the localized name and correct icons for "Network Neighborhood"
			LPITEMIDLIST pidl = nullptr;
			if (SUCCEEDED(SHGetFolderLocation(NULL, CSIDL_NETWORK, NULL, 0, &pidl)))
			{
				SHFILEINFO sfi = {0};
				if (SHGetFileInfo((LPCTSTR) pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_DISPLAYNAME))
					pItem->m_sRelativePath = sfi.szDisplayName;
				nIcon = GetIconIndex(pidl);
				nSelIcon = GetSelIconIndex(pidl);
				CoTaskMemFree(pidl);
			}
			
			//Add it to the tree control
			m_hNetworkRoot = InsertFileItem(TVI_ROOT, pItem, false, nIcon, nSelIcon, false);
			SetHasSharedChildren(m_hNetworkRoot);
		}
	}
	else
	{
		DisplayPath(m_sRootFolder, TVI_ROOT, false);
	}
	
	//Reselect the initially selected item
	if (hSelItem)
		SetSelectedPath(sItem, bExpanded);
}

tstring FolderTree::ItemToPath(HTREEITEM hItem) const
{
	tstring sPath;
	if (hItem)
	{
		FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hItem);
		//ASSERT(pItem);
		sPath = pItem->m_sFQPath;
	}
	return sPath;
}

bool FolderTree::IsExpanded(HTREEITEM hItem)
{
	TVITEM tvItem;
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_HANDLE | TVIF_STATE;
	return (GetItem(&tvItem) && (tvItem.state & TVIS_EXPANDED));
}

void FolderTree::Clear()
{
	//Delete all the items
	DeleteAllItems();
	
	//Reset the member variables we have
	m_hMyComputerRoot = NULL;
	m_hNetworkRoot = NULL;
	m_hRootedFolder = NULL;
}

int FolderTree::GetIconIndex(HTREEITEM hItem)
{
	TV_ITEM tvi = {0};
	tvi.mask = TVIF_IMAGE;
	tvi.hItem = hItem;
	if (GetItem(&tvi))
		return tvi.iImage;
	else
		return -1;
}

int FolderTree::GetIconIndex(const tstring &sFilename)
{
	//Retreive the icon index for a specified file/folder
	SHFILEINFO sfi  = {0};
	SHGetFileInfo(sFilename.c_str(), 0, &sfi, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	return sfi.iIcon;
}

int FolderTree::GetIconIndex(LPITEMIDLIST lpPIDL)
{
	SHFILEINFO sfi  = {0};
	SHGetFileInfo((LPCTSTR)lpPIDL, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_LINKOVERLAY);
	return sfi.iIcon;
}

int FolderTree::GetSelIconIndex(LPITEMIDLIST lpPIDL)
{
	SHFILEINFO sfi  = {0};
	SHGetFileInfo((LPCTSTR)lpPIDL, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_OPENICON);
	return sfi.iIcon;
}

int FolderTree::GetSelIconIndex(HTREEITEM hItem)
{
	TV_ITEM tvi  = {0};
	tvi.mask = TVIF_SELECTEDIMAGE;
	tvi.hItem = hItem;
	if (GetItem(&tvi))
		return tvi.iSelectedImage;
	else
		return -1;
}

int FolderTree::GetSelIconIndex(const tstring &sFilename)
{
	//Retreive the icon index for a specified file/folder
	SHFILEINFO sfi  = {0};
	SHGetFileInfo(sFilename.c_str(), 0, &sfi, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_OPENICON | SHGFI_SMALLICON);
	return sfi.iIcon;
}

HTREEITEM FolderTree::InsertFileItem(HTREEITEM hParent, FolderTreeItemInfo *pItem, bool bShared, int nIcon, int nSelIcon, bool bCheckForChildren)
{
	tstring sLabel;
	
	//Correct the label if need be
	if (IsDrive(pItem->m_sFQPath) && m_bShowDriveLabels)
		sLabel = GetDriveLabel(pItem->m_sFQPath);
	else
		sLabel = GetCorrectedLabel(pItem);
		
	//Add the actual item
	TV_INSERTSTRUCT tvis  = {0};
	tvis.hParent = hParent;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM;
	tvis.item.iImage = nIcon;
	tvis.item.iSelectedImage = nSelIcon;
	
	tvis.item.lParam = (LPARAM) pItem;
	tvis.item.pszText = (LPWSTR)sLabel.c_str();
	if (bCheckForChildren)
		tvis.item.cChildren = HasGotSubEntries(pItem->m_sFQPath);
	else
		tvis.item.cChildren = TRUE;
		
	if (bShared)
	{
		tvis.item.mask |= TVIF_STATE;
		tvis.item.stateMask |= TVIS_OVERLAYMASK;
		tvis.item.state |= INDEXTOOVERLAYMASK(1); //1 is the index for the shared overlay image
	}
	
	const HTREEITEM hItem = InsertItem(&tvis);
	
	string path = Text::fromT(pItem->m_sFQPath);
	Util::appendPathSeparator(path);
	if (!path.empty())
	{
		const bool bChecked = ShareManager::getInstance()->isDirectoryShared(path);
		SetChecked(hItem, bChecked);
		if (!bChecked)
			SetHasSharedChildren(hItem);
	}
	return hItem;
}

void FolderTree::DisplayDrives(HTREEITEM hParent, bool bUseSetRedraw /* = true */)
{
	//CWaitCursor waitCursor;
	
	//Speed up the job by turning off redraw
	if (bUseSetRedraw)
		SetRedraw(FALSE);
		
	//Enumerate the drive letters and add them to the tree control
	DWORD dwDrives = GetLogicalDrives();
	DWORD dwMask = 1;
	for (int i = 0; i < 32; i++)
	{
		if (dwDrives & dwMask)
		{
			tstring sDrive;
			sDrive = (TCHAR)('A' + i);
			sDrive += _T(":\\");
			
			//check if this drive is one of the types to hide
			if (CanDisplayDrive(sDrive))
			{
				FolderTreeItemInfo* pItem = new FolderTreeItemInfo(sDrive, sDrive);
				//Insert the item into the view
				InsertFileItem(hParent, pItem, m_bShowSharedUsingDifferentIcon && IsShared(sDrive), GetIconIndex(sDrive), GetSelIconIndex(sDrive), true);
			}
		}
		dwMask <<= 1;
	}
	
	if (bUseSetRedraw)
		SetRedraw(TRUE);
}

void FolderTree::DisplayPath(const tstring &sPath, HTREEITEM hParent, bool bUseSetRedraw /* = true */)
{
	CWaitCursor waitCursor;
	
	//Speed up the job by turning off redraw
	if (bUseSetRedraw)
		SetRedraw(FALSE);
		
	//Remove all the items currently under hParent
	HTREEITEM hChild = GetChildItem(hParent);
	while (hChild)
	{
		DeleteItem(hChild);
		hChild = GetChildItem(hParent);
	}
	
	//Should we display the root folder
	if (m_bShowRootedFolder && (hParent == TVI_ROOT))
	{
		FolderTreeItemInfo* pItem = new FolderTreeItemInfo(m_sRootFolder, m_sRootFolder);
		m_hRootedFolder = InsertFileItem(TVI_ROOT, pItem, false, GetIconIndex(m_sRootFolder), GetSelIconIndex(m_sRootFolder), true);
		Expand(m_hRootedFolder, TVE_EXPAND);
		return;
	}
	
	//find all the directories underneath sPath
	int nDirectories = 0;
	
	tstring sFile = sPath;
	Util::appendPathSeparator(sFile);
	
	WIN32_FIND_DATA fData;
	HANDLE hFind = FindFirstFileEx((sFile + _T('*')).c_str(),
	                               CompatibilityManager::findFileLevel,
	                               &fData,
	                               FindExSearchNameMatch,
	                               NULL,
	                               CompatibilityManager::findFileFlags);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			const tstring filename = fData.cFileName;
			if ((fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !Util::isReservedDirName(filename))
			{
				++nDirectories;
				const tstring fullPath = sFile + filename;
				FolderTreeItemInfo* pItem = new FolderTreeItemInfo(fullPath, fData.cFileName);
				const int icon = GetIconIndex(fullPath);
				const int selIcon = GetSelIconIndex(fullPath);
				InsertFileItem(hParent, pItem, m_bShowSharedUsingDifferentIcon && IsShared(fullPath), icon, selIcon, true);
			}
		}
		while (FindNextFile(hFind, &fData));
		FindClose(hFind);
	}
	
	//Now sort the items we just added
	TVSORTCB tvsortcb = {0};
	tvsortcb.hParent = hParent;
	tvsortcb.lpfnCompare = CompareByFilenameNoCase;
	tvsortcb.lParam = 0;
	SortChildrenCB(&tvsortcb);
	
	//If no items were added then remove the "+" indicator from hParent
	if (nDirectories == 0)
		SetHasPlusButton(hParent, FALSE);
		
	//Turn back on the redraw flag
	if (bUseSetRedraw)
		SetRedraw(TRUE);
}

HTREEITEM FolderTree::SetSelectedPath(const tstring &sPath, bool bExpanded /* = false */)
{
	tstring sSearch = Text::toLower(sPath);
	if (sSearch.empty())
	{
		//TRACE(_T("Cannot select a empty path\n"));
		return NULL;
	}
	
	//Remove initial part of path if the root folder is setup
	tstring sRootFolder = Text::toLower(m_sRootFolder);
	const tstring::size_type nRootLength = sRootFolder.size();
	if (nRootLength)
	{
		if (sSearch.find(sRootFolder) != 0)
			// TODO Either inefficent or wrong usage of string::find(). string::compare() will be faster if string::find's
			// result is compared with 0, because it will not scan the whole string. If your intention is to check that there
			// are no findings in the string, you should compare with std::string::npos.
		{
			//TRACE(_T("Could not select the path %s as the root has been configued as %s\n"), sPath, m_sRootFolder);
			return NULL;
		}
		sSearch = sSearch.substr(nRootLength);
	}
	
	//Remove trailing "\" from the path
	Util::removePathSeparator(sSearch);
	
	if (sSearch.empty())
		return nullptr;
		
	CLockRedraw<> lockRedraw(m_hWnd);
	
	HTREEITEM hItemFound = TVI_ROOT;
	if (nRootLength && m_hRootedFolder)
		hItemFound = m_hRootedFolder;
	bool bDriveMatch = sRootFolder.empty();
	bool bNetworkMatch = m_bDisplayNetwork && sSearch.size() > 2 && sSearch.find(_T("\\\\")) == 0;
	if (bNetworkMatch)
	{
		bDriveMatch = false;
		
		//Working here
		bool bHasPlus = HasPlusButton(m_hNetworkRoot);
		bool bHasChildren = (GetChildItem(m_hNetworkRoot) != NULL);
		
		if (bHasPlus && !bHasChildren)
			DoExpand(m_hNetworkRoot);
		else
			Expand(m_hNetworkRoot, TVE_EXPAND);
			
		hItemFound = FindServersNode(m_hNetworkRoot);
		sSearch = sSearch.substr(2);
	}
	if (bDriveMatch)
	{
		if (m_hMyComputerRoot)
		{
			//Working here
			bool bHasPlus = HasPlusButton(m_hMyComputerRoot);
			bool bHasChildren = (GetChildItem(m_hMyComputerRoot) != NULL);
			
			if (bHasPlus && !bHasChildren)
				DoExpand(m_hMyComputerRoot);
			else
				Expand(m_hMyComputerRoot, TVE_EXPAND);
				
			hItemFound = m_hMyComputerRoot;
		}
	}
	
	size_t nFound = sSearch.find(_T('\\'));
	while (nFound != tstring::npos)
	{
		tstring sMatch;
		if (bDriveMatch)
		{
			sMatch = sSearch.substr(0, nFound + 1);
			bDriveMatch = false;
		}
		else
			sMatch = sSearch.substr(0, nFound);
			
		hItemFound = FindSibling(hItemFound, sMatch);
		if (hItemFound == NULL)
			break;
		else if (!IsDrive(sPath))
		{
			SelectItem(hItemFound);
			
			//Working here
			bool bHasPlus = HasPlusButton(hItemFound);
			bool bHasChildren = (GetChildItem(hItemFound) != NULL);
			
			if (bHasPlus && !bHasChildren)
				DoExpand(hItemFound);
			else
				Expand(hItemFound, TVE_EXPAND);
		}
		
		sSearch = sSearch.substr(nFound - 1);
		nFound = sSearch.find(_T('\\'));
	};
	
	//The last item
	if (hItemFound)
	{
		if (sSearch.size())
			hItemFound = FindSibling(hItemFound, sSearch);
		if (hItemFound)
			SelectItem(hItemFound);
			
		if (bExpanded)
		{
			//Working here
			bool bHasPlus = HasPlusButton(hItemFound);
			bool bHasChildren = (GetChildItem(hItemFound) != NULL);
			
			if (bHasPlus && !bHasChildren)
				DoExpand(hItemFound);
			else
				Expand(hItemFound, TVE_EXPAND);
		}
	}
	
	return hItemFound;
}

bool FolderTree::IsDrive(HTREEITEM hItem)
{
	return IsDrive(ItemToPath(hItem));
}

bool FolderTree::IsDrive(const tstring &sPath)
{
	return sPath.size() == 3 && sPath[1] == _T(':') && sPath[2] == _T('\\');
}

tstring FolderTree::GetDriveLabel(const tstring &drive)
{
#ifdef _UNICODE
	const wstring& path = drive;
#else
	wstring path;
	Text::utf8ToWide(drive, path);
#endif
	tstring result;
	//Try to find the item directory using ParseDisplayName
	LPITEMIDLIST pidl = nullptr;
	if (m_pShellFolder && SUCCEEDED(m_pShellFolder->ParseDisplayName(nullptr, nullptr, const_cast<wchar_t*>(path.c_str()), nullptr, &pidl, nullptr)))
	{
		SHFILEINFO sfi = {0};
		if (SHGetFileInfo((LPCTSTR) pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_DISPLAYNAME))
			result = sfi.szDisplayName;
		CoTaskMemFree(pidl);
	}
	return result.empty() ? drive : result;
}

bool FolderTree::HasGotSubEntries(const tstring &directory)
{
	if (directory.empty())
		return false;
		
	if (DriveHasRemovableMedia(directory))
	{
		return true; //we do not bother searching for files on drives
		//which have removable media as this would cause
		//the drive to spin up, which for the case of a
		//floppy is annoying
	}
	else
	{
		//First check to see if there is any sub directories
		tstring sFile;
		if (directory[directory.size() - 1] == _T('\\'))
			sFile = directory + _T("*.*");
		else
			sFile = directory + _T("\\*.*");
			
		WIN32_FIND_DATA fData;
		HANDLE hFind = FindFirstFileEx(sFile.c_str(),
		                               CompatibilityManager::findFileLevel,
		                               &fData,
		                               FindExSearchNameMatch,
		                               NULL,
		                               CompatibilityManager::findFileFlags);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				const tstring cFileName = fData.cFileName;
				if ((fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !Util::isReservedDirName(cFileName))
				{
					FindClose(hFind);
					return true;
				}
			}
			while (FindNextFile(hFind, &fData));
			FindClose(hFind);
		}
	}
	
	return false;
}

bool FolderTree::CanDisplayDrive(const tstring &sDrive)
{
	//check if this drive is one of the types to hide
	bool bDisplay = true;
	UINT nDrive = GetDriveType(sDrive.c_str());
	switch (nDrive)
	{
		case DRIVE_REMOVABLE:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_REMOVABLE)
				bDisplay = false;
			break;
		}
		case DRIVE_FIXED:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_FIXED)
				bDisplay = false;
			break;
		}
		case DRIVE_REMOTE:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_REMOTE)
				bDisplay = false;
			break;
		}
		case DRIVE_CDROM:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_CDROM)
				bDisplay = false;
			break;
		}
		case DRIVE_RAMDISK:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_RAMDISK)
				bDisplay = false;
			break;
		}
		default:
		{
			break;
		}
	}
	
	return bDisplay;
}

bool FolderTree::IsShared(const tstring &path) const
{
	//Defer all the work to the share enumerator class
	return theSharedEnumerator.IsShared(path);
}

int CALLBACK FolderTree::CompareByFilenameNoCase(LPARAM lParam1, LPARAM lParam2, LPARAM /*lParamSort*/)
{
	FolderTreeItemInfo* pItem1 = (FolderTreeItemInfo*) lParam1;
	FolderTreeItemInfo* pItem2 = (FolderTreeItemInfo*) lParam2;
	
	return stricmp(pItem1->m_sRelativePath, pItem2->m_sRelativePath);
}

void FolderTree::SetHasPlusButton(HTREEITEM hItem, bool bHavePlus)
{
	//Remove all the child items from the parent
	TV_ITEM tvItem = {0};
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_CHILDREN;
	tvItem.cChildren = bHavePlus;
	SetItem(&tvItem);
}

bool FolderTree::HasPlusButton(HTREEITEM hItem)
{
	TVITEM tvItem;
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_HANDLE | TVIF_CHILDREN;
	return GetItem(&tvItem) && (tvItem.cChildren != 0);
}

void FolderTree::DoExpand(HTREEITEM hItem)
{
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hItem);
	
	//Reset the drive node if the drive is empty or the media has changed
	if (IsMediaValid(pItem->m_sFQPath))
	{
		//Delete the item if the path is no longer valid
		if (IsFolder(pItem->m_sFQPath))
		{
			//Add the new items to the tree if it does not have any child items
			//already
			if (!GetChildItem(hItem))
				DisplayPath(pItem->m_sFQPath, hItem);
		}
		else if (hItem == m_hMyComputerRoot)
		{
			//Display an hour glass as this may take some time
			//CWaitCursor waitCursor;
			
			//Enumerate the local drive letters
			DisplayDrives(m_hMyComputerRoot, FALSE);
		}
		else if ((hItem == m_hNetworkRoot) || (pItem->m_pNetResource))
		{
			//Display an hour glass as this may take some time
			//CWaitCursor waitCursor;
			
			//Enumerate the network resources
			EnumNetwork(hItem);
		}
		else
		{
			//Before we delete it see if we are the only child item
			HTREEITEM hParent = GetParentItem(hItem);
			
			//Delete the item
			DeleteItem(hItem);
			
			//Remove all the child items from the parent
			SetHasPlusButton(hParent, false);
		}
	}
	else
	{
		//Display an hour glass as this may take some time
		//CWaitCursor waitCursor;
		
		//Collapse the drive node and remove all the child items from it
		Expand(hItem, TVE_COLLAPSE);
		DeleteChildren(hItem, true);
	}
}

HTREEITEM FolderTree::FindServersNode(HTREEITEM hFindFrom) const
{
	if (m_bDisplayNetwork)
	{
		//Try to find some "servers" in the child items of hFindFrom
		HTREEITEM hChild = GetChildItem(hFindFrom);
		while (hChild)
		{
			FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hChild);
			
			if (pItem->m_pNetResource)
			{
				//Found a share
				if (pItem->m_pNetResource->dwDisplayType == RESOURCEDISPLAYTYPE_SERVER)
					return hFindFrom;
			}
			
			//Get the next sibling for the next loop around
			hChild = GetNextSiblingItem(hChild);
		}
		
		//Ok, since we got here, we did not find any servers in any of the child nodes of this
		//item. In this case we need to call ourselves recursively to find one
		hChild = GetChildItem(hFindFrom);
		while (hChild)
		{
			HTREEITEM hFound = FindServersNode(hChild);
			if (hFound)
				return hFound;
				
			//Get the next sibling for the next loop around
			hChild = GetNextSiblingItem(hChild);
		}
	}
	
	//If we got as far as here then no servers were found.
	return nullptr;
}

HTREEITEM FolderTree::FindSibling(HTREEITEM hParent, const tstring &sItem) const
{
	HTREEITEM hChild = GetChildItem(hParent);
	while (hChild)
	{
		FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hChild);
		
		if (stricmp(pItem->m_sRelativePath, sItem) == 0)
			return hChild;
		hChild = GetNextItem(hChild, TVGN_NEXT);
	}
	return nullptr;
}

bool FolderTree::DriveHasRemovableMedia(const tstring &sPath)
{
	bool bRemovableMedia = false;
	if (IsDrive(sPath))
	{
		UINT nDriveType = GetDriveType(sPath.c_str());
		bRemovableMedia = ((nDriveType == DRIVE_REMOVABLE) ||
		                   (nDriveType == DRIVE_CDROM));
	}
	
	return bRemovableMedia;
}

bool FolderTree::IsMediaValid(const tstring &sDrive)
{
	//return TRUE if the drive does not support removable media
	UINT nDriveType = GetDriveType(sDrive.c_str());
	if ((nDriveType != DRIVE_REMOVABLE) && (nDriveType != DRIVE_CDROM))
		return true;
		
	//Return FALSE if the drive is empty (::GetVolumeInformation fails)
	DWORD dwSerialNumber;
	int nDrive = sDrive[0] - _T('A');
	if (GetSerialNumber(sDrive, dwSerialNumber))
		m_dwMediaID[nDrive] = dwSerialNumber;
	else
	{
		m_dwMediaID[nDrive] = 0xFFFFFFFF;
		return false;
	}
	
	//Also return FALSE if the disk's serial number has changed
	if ((m_dwMediaID[nDrive] != dwSerialNumber) &&
	        (m_dwMediaID[nDrive] != 0xFFFFFFFF))
	{
		m_dwMediaID[nDrive] = 0xFFFFFFFF;
		return false;
	}
	
	return true;
}

bool FolderTree::IsFolder(const tstring &sPath)
{
	DWORD dwAttributes = GetFileAttributes(sPath.c_str());
	return dwAttributes != INVALID_FILE_ATTRIBUTES && (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FolderTree::EnumNetwork(HTREEITEM hParent)
{
	//What will be the return value from this function
	bool bGotChildren = false;

	//Check if the item already has a network resource and use it.
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hParent);

	NETRESOURCE* pNetResource = pItem->m_pNetResource;

	//Setup for the network enumeration
	HANDLE hEnum;
	DWORD dwResult = WNetOpenEnum(pNetResource ? RESOURCE_GLOBALNET : RESOURCE_CONTEXT, m_dwNetworkItemTypes,
	                              0, pNetResource ? pNetResource : NULL, &hEnum);

	//Was the read sucessful
	if (dwResult != NO_ERROR)
	{
		//TRACE(_T("Cannot enumerate network drives, Error:%d\n"), dwResult);
		return false;
	}

	//Do the network enumeration
	DWORD cbBuffer = 16384;

	bool bSuccess = false;
	LPNETRESOURCE lpnrDrv = nullptr;
	DWORD cEntries = 0;
	while (true)
	{
		lpnrDrv = (LPNETRESOURCE) operator new(cbBuffer);
		cEntries = 0xFFFFFFFF;
		dwResult = WNetEnumResource(hEnum, &cEntries, lpnrDrv, &cbBuffer);

		if (dwResult == ERROR_MORE_DATA)
		{
			operator delete(lpnrDrv);
			cbBuffer *= 2;
		}
		else
		{
			if (dwResult == NO_ERROR) bSuccess = true;
			break;
		}
	}

	//Enumeration successful?
	if (bSuccess)
	{
		//Scan through the results
		for (DWORD i = 0; i < cEntries; i++)
		{
			tstring sNameRemote;
			if (lpnrDrv[i].lpRemoteName != NULL)
				sNameRemote = lpnrDrv[i].lpRemoteName;
			else
				sNameRemote = lpnrDrv[i].lpComment;
				
			//Remove leading back slashes
			if (!sNameRemote.empty() && sNameRemote[0] == _T('\\'))
				sNameRemote = sNameRemote.substr(1);
			if (!sNameRemote.empty() && sNameRemote[0] == _T('\\'))
				sNameRemote = sNameRemote.substr(1);
				
			//Setup the item data for the new item
			FolderTreeItemInfo* pItem = new FolderTreeItemInfo;
			pItem->m_pNetResource = new NETRESOURCE;
			memcpy(pItem->m_pNetResource, &lpnrDrv[i], sizeof(NETRESOURCE));

			if (lpnrDrv[i].lpLocalName)
				pItem->m_pNetResource->lpLocalName = _tcsdup(lpnrDrv[i].lpLocalName);
			if (lpnrDrv[i].lpRemoteName)
				pItem->m_pNetResource->lpRemoteName = _tcsdup(lpnrDrv[i].lpRemoteName);
			if (lpnrDrv[i].lpComment)
				pItem->m_pNetResource->lpComment = _tcsdup(lpnrDrv[i].lpComment);
			if (lpnrDrv[i].lpProvider)
				pItem->m_pNetResource->lpProvider = _tcsdup(lpnrDrv[i].lpProvider);
			if (lpnrDrv[i].lpRemoteName)
				pItem->m_sFQPath = lpnrDrv[i].lpRemoteName;
			else
				pItem->m_sFQPath = sNameRemote;

			pItem->m_sRelativePath = std::move(sNameRemote);
			pItem->m_bNetworkNode = true;

			//Display a share and the appropiate icon
			if (lpnrDrv[i].dwDisplayType == RESOURCEDISPLAYTYPE_SHARE)
			{
				//Display only the share name
				tstring::size_type nPos = pItem->m_sRelativePath.find(_T('\\'));
				if (nPos != tstring::npos)
					pItem->m_sRelativePath = pItem->m_sRelativePath.substr(nPos + 1);
					
				//Now add the item into the control
				InsertFileItem(hParent, pItem, m_bShowSharedUsingDifferentIcon, GetIconIndex(pItem->m_sFQPath),
				               GetSelIconIndex(pItem->m_sFQPath), TRUE);
			}
			else if (lpnrDrv[i].dwDisplayType == RESOURCEDISPLAYTYPE_SERVER)
			{
				//Now add the item into the control
				tstring sServer = _T("\\\\");
				sServer += pItem->m_sRelativePath;
				InsertFileItem(hParent, pItem, false, GetIconIndex(sServer), GetSelIconIndex(sServer), false);
			}
			else
			{
				//Now add the item into the control
				//Just use the generic Network Neighborhood icons for everything else
				LPITEMIDLIST pidl = nullptr;
				int nIcon = 0xFFFF;// TODO WTF?
				int nSelIcon = nIcon;
				if (SUCCEEDED(SHGetFolderLocation(NULL, CSIDL_NETWORK, NULL, 0, &pidl)))
				{
					nIcon = GetIconIndex(pidl);
					nSelIcon = GetSelIconIndex(pidl);
					CoTaskMemFree(pidl);
				}
				InsertFileItem(hParent, pItem, false, nIcon, nSelIcon, false);
			}
			bGotChildren = true;
		}
	}
	/*
	else
		TRACE(_T("Cannot complete network drive enumeration, Error:%d\n"), dwResult);
	*/

	//Clean up the enumeration handle
	WNetCloseEnum(hEnum);

	//Free up the heap memory we have used
	operator delete(lpnrDrv);

	//Return whether or not we added any items
	return bGotChildren;
}

int FolderTree::DeleteChildren(HTREEITEM hItem, bool bUpdateChildIndicator)
{
	int nCount = 0;
	HTREEITEM hChild = GetChildItem(hItem);
	while (hChild)
	{
		//Get the next sibling before we delete the current one
		HTREEITEM hNextItem = GetNextSiblingItem(hChild);
		
		//Delete the current child
		DeleteItem(hChild);
		
		//Get ready for the next loop
		hChild = hNextItem;
		++nCount;
	}
	
	//Also update its indicator to suggest that their is children
	if (bUpdateChildIndicator)
		SetHasPlusButton(hItem, (nCount != 0));
		
	return nCount;
}

bool FolderTree::GetSerialNumber(const tstring &sDrive, DWORD &dwSerialNumber)
{
	return (bool) GetVolumeInformation(sDrive.c_str(), NULL, 0, &dwSerialNumber, NULL, NULL, NULL, 0);
}

LRESULT FolderTree::OnSelChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL &bHandled)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pnmh;
	
	//Nothing selected
	if (pNMTreeView->itemNew.hItem == NULL)
	{
		bHandled = FALSE;
		return 0;
	}
	
	//Check to see if the current item is valid, if not then delete it (Exclude network items from this check)
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(pNMTreeView->itemNew.hItem);
	//ASSERT(pItem);
	tstring sPath = pItem->m_sFQPath;
	if ((pNMTreeView->itemNew.hItem != m_hNetworkRoot) && (pItem->m_pNetResource == NULL) &&
	        (pNMTreeView->itemNew.hItem != m_hMyComputerRoot) && !IsDrive(sPath) && (GetFileAttributes(sPath.c_str()) == INVALID_FILE_ATTRIBUTES))
	{
		//Before we delete it see if we are the only child item
		HTREEITEM hParent = GetParentItem(pNMTreeView->itemNew.hItem);
		
		//Delete the item
		DeleteItem(pNMTreeView->itemNew.hItem);
		
		//Remove all the child items from the parent
		SetHasPlusButton(hParent, false);
		
		bHandled = FALSE; //Allow the message to be reflected again
		return 1;
	}
	
	//Remeber the serial number for this item (if it is a drive)
	if (IsDrive(sPath))
	{
		int nDrive = sPath[0] - _T('A');
		GetSerialNumber(sPath, m_dwMediaID[nDrive]);
	}
	
	bHandled = FALSE; //Allow the message to be reflected again
	
	return 0;
}

LRESULT FolderTree::OnItemExpanding(int /*idCtrl*/, LPNMHDR pnmh, BOOL &bHandled)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pnmh;
	if (pNMTreeView->action == TVE_EXPAND)
	{
		bool bHasPlus = HasPlusButton(pNMTreeView->itemNew.hItem);
		bool bHasChildren = (GetChildItem(pNMTreeView->itemNew.hItem) != NULL);
		
		if (bHasPlus && !bHasChildren)
			DoExpand(pNMTreeView->itemNew.hItem);
	}
	else if (pNMTreeView->action == TVE_COLLAPSE)
	{
		FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(pNMTreeView->itemNew.hItem);
		//ASSERT(pItem);
		
		//Display an hour glass as this may take some time
		//CWaitCursor waitCursor;
		
		//Collapse the node and remove all the child items from it
		Expand(pNMTreeView->itemNew.hItem, TVE_COLLAPSE);
		
		//Never uppdate the child indicator for a network node which is not a share
		bool bUpdateChildIndicator = true;
		if (pItem->m_bNetworkNode)
		{
			if (pItem->m_pNetResource)
				bUpdateChildIndicator = (pItem->m_pNetResource->dwDisplayType == RESOURCEDISPLAYTYPE_SHARE);
			else
				bUpdateChildIndicator = false;
		}
		DeleteChildren(pNMTreeView->itemNew.hItem, bUpdateChildIndicator);
	}
	
	bHandled = FALSE; //Allow the message to be reflected again
	return 0;
}

LRESULT FolderTree::OnDeleteItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL &bHandled)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pnmh;
	if (pNMTreeView->itemOld.hItem != TVI_ROOT)
	{
		FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) pNMTreeView->itemOld.lParam;
		delete pItem;
	}
	bHandled = FALSE; //Allow the message to be reflected again
	return 0;
}

LRESULT FolderTree::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_SPACE:
			HTREEITEM htItem = GetSelectedItem();
			
			if (!GetChecked(htItem))
				return OnChecked(htItem, bHandled);
			else
				return OnUnChecked(htItem, bHandled);
	}
	return 0;
}

bool FolderTree::GetChecked(HTREEITEM hItem) const
{
	UINT u = GetItemState(hItem, TVIS_STATEIMAGEMASK);
	return (u & INDEXTOSTATEIMAGEMASK(TVIF_IMAGE | TVIF_PARAM | TVIF_STATE)) ? true : false;
}

BOOL FolderTree::SetChecked(HTREEITEM hItem, bool fCheck)
{
	TVITEM item;
	item.mask = TVIF_HANDLE | TVIF_STATE;
	item.hItem = hItem;
	item.stateMask = TVIS_STATEIMAGEMASK;
	
	/*
	Since state images are one-based, 1 in this macro turns the check off, and
	2 turns it on.
	*/
	item.state = INDEXTOSTATEIMAGEMASK((fCheck ? TVIF_IMAGE : TVIF_TEXT));
	
	return SetItem(&item);
}

LRESULT FolderTree::OnClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled)
{
	DWORD dwPos = GetMessagePos();
	POINT ptPos;
	ptPos.x = GET_X_LPARAM(dwPos);
	ptPos.y = GET_Y_LPARAM(dwPos);
	
	ScreenToClient(&ptPos);
	
	UINT uFlags;
	HTREEITEM htItemClicked = HitTest(ptPos, &uFlags);
	
	// if the item's checkbox was clicked ...
	if (uFlags & TVHT_ONITEMSTATEICON)
	{
		// retrieve is's soon-to-be former state
		if (!GetChecked(htItemClicked))
			return OnChecked(htItemClicked, bHandled);
		else
			return OnUnChecked(htItemClicked, bHandled);
	}
	
	bHandled = FALSE;
	return 0;
}

LRESULT FolderTree::OnChecked(HTREEITEM hItem, BOOL &bHandled)
{
	CWaitCursor waitCursor;
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hItem);
	if (!validatePath(pItem->m_sFQPath))
	{
		// no checking myComp or network
		bHandled = TRUE;
		return 1;
	}
	
	HTREEITEM hSharedParent = HasSharedParent(hItem);
	// if a parent is checked then this folder should be removed from the ex list
	if (hSharedParent != NULL)
	{
		ShareParentButNotSiblings(hItem);
	}
	else
	{
		// if no parent folder is checked then this is a new root dir
		try
		{
			LineDlg virt;
			virt.title = TSTRING(VIRTUAL_NAME);
			virt.description = TSTRING(VIRTUAL_NAME_LONG);
			virt.allowEmpty = false;
			virt.checkBox = virt.checked = true;
			virt.checkBoxText = ResourceManager::ADD_TO_DEFAULT_SHARE_GROUP;
			virt.icon = IconBitmaps::FINISHED_UPLOADS;

			tstring path = pItem->m_sFQPath;
			Util::appendPathSeparator(path);
			virt.line = Text::toT(ShareManager::validateVirtual(Util::getLastDir(Text::fromT(path))));

			if (virt.DoModal() == IDOK)
			{
				string realPath = Text::fromT(path);
				ShareManager* sm = ShareManager::getInstance();
				sm->addDirectory(realPath, Text::fromT(virt.line));
				if (virt.checked)
				{
					CID id;
					list<string> dirs;
					sm->getShareGroupDirectories(id, dirs);
					dirs.push_back(realPath);
					sm->updateShareGroup(id, Util::emptyString, dirs);
				}
				if (listener) listener->onChange();
			}
			else
			{
				bHandled = TRUE;
				return 1;
			}
			UpdateParentItems(hItem);
		}
		catch (const ShareException& e)
		{
			MessageBox(Text::toT(e.getError()).c_str(), getAppNameVerT().c_str(), MB_ICONSTOP | MB_OK);
			bHandled = TRUE;
			return 1;
		}
	}
	
	UpdateChildItems(hItem, true);
	return 0;
}

LRESULT FolderTree::OnUnChecked(HTREEITEM hItem, BOOL& /*bHandled*/)
{
	CWaitCursor waitCursor; //-V808
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hItem);
	
	HTREEITEM hSharedParent = HasSharedParent(hItem);
	ShareManager* sm = ShareManager::getInstance();
	// if no parent is checked remove this root folder from share
	if (hSharedParent == NULL)
	{
		string path = Text::fromT(pItem->m_sFQPath);
		Util::appendPathSeparator(path);
		sm->removeExcludeFolder(path);
		sm->removeDirectory(path);
		UpdateParentItems(hItem);
		if (listener) listener->onChange();
	}
	else if (GetChecked(GetParentItem(hItem)))
	{
		// if the parent is checked add this folder to excludes
		string path = Text::fromT(pItem->m_sFQPath);
		Util::appendPathSeparator(path);
		sm->addExcludeFolder(path);
		if (listener) listener->onChange();
	}
	
	UpdateChildItems(hItem, false);	
	return 0;
}

bool FolderTree::GetHasSharedChildren(HTREEITEM hItem)
{
	if (!hItem) return false;

	string searchStr;
	string::size_type startPos = 0;
	
	if (hItem == m_hMyComputerRoot)
	{
		startPos = 1;
		searchStr = ":\\";
	}
	else if (hItem == m_hNetworkRoot)
		searchStr = "\\\\";
	else
	{
		FolderTreeItemInfo* pItem  = (FolderTreeItemInfo*) GetItemData(hItem);
		searchStr = Text::fromT(pItem->m_sFQPath);
		
		if (searchStr.empty())
			return false;
	}
		
	vector<ShareManager::SharedDirInfo> dirs;
	ShareManager::getInstance()->getDirectories(dirs);
	
	for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
	{
		if (i->isExcluded) continue;
		const string& sharedPath = i->realPath;
		if (sharedPath.length() > searchStr.length() + startPos)
		{
			if (stricmp(sharedPath.substr(startPos, searchStr.length()), searchStr) == 0)
			{
				if (searchStr.length() <= 3)
				{
					bool exists = File::isExist(sharedPath);
					return exists;
				}
				if (sharedPath[searchStr.length()] == '\\')
				{
					bool exists = File::isExist(sharedPath);
					return exists;
				}
				return false;
			}
		}
	}
	return false;
}

void FolderTree::SetHasSharedChildren(HTREEITEM hItem)
{
	SetHasSharedChildren(hItem, GetHasSharedChildren(hItem));
}

void FolderTree::SetHasSharedChildren(HTREEITEM hItem, bool bHasSharedChildren)
{
	if (bHasSharedChildren)
		SetItemState(hItem, TVIS_BOLD, TVIS_BOLD);
	else
		SetItemState(hItem, 0, TVIS_BOLD);
}

void FolderTree::UpdateChildItems(HTREEITEM hItem, bool bChecked)
{
	HTREEITEM hChild = GetChildItem(hItem);
	while (hChild)
	{
		SetChecked(hChild, bChecked);
		UpdateChildItems(hChild, bChecked);
		hChild = GetNextSiblingItem(hChild);
	}
}

void FolderTree::UpdateParentItems(HTREEITEM hItem)
{
	HTREEITEM hParent = GetParentItem(hItem);
	if (hParent != nullptr && HasSharedParent(hParent) == NULL)
	{
		SetHasSharedChildren(hParent);
		UpdateParentItems(hParent);
	}
}

HTREEITEM FolderTree::HasSharedParent(HTREEITEM hItem)
{
	HTREEITEM hParent = GetParentItem(hItem);
	while (hParent != nullptr)
	{
		if (GetChecked(hParent))
			return hParent;
			
		hParent = GetParentItem(hParent);
	}
	
	return NULL;
}

void FolderTree::ShareParentButNotSiblings(HTREEITEM hItem)
{
	FolderTreeItemInfo* pItem;
	HTREEITEM hParent = GetParentItem(hItem);
	ShareManager* sm = ShareManager::getInstance();
	if (!GetChecked(hParent))
	{
		SetChecked(hParent, true);
		pItem = (FolderTreeItemInfo*) GetItemData(hParent);
		string path = Text::fromT(pItem->m_sFQPath);
		Util::appendPathSeparator(path);
		sm->removeExcludeFolder(path);
		if (listener) listener->onChange();
		
		ShareParentButNotSiblings(hParent);
		
		HTREEITEM hChild = GetChildItem(hParent);
		while (hChild)
		{
			HTREEITEM hNextItem = GetNextSiblingItem(hChild);
			if (!GetChecked(hChild))
			{
				pItem = (FolderTreeItemInfo*) GetItemData(hChild);
				if (hChild != hItem)
				{
					string path = Text::fromT(pItem->m_sFQPath);
					Util::appendPathSeparator(path);
					sm->addExcludeFolder(path);
				}
			}
			hChild = hNextItem;
		}
	}
	else
	{
		pItem = (FolderTreeItemInfo*) GetItemData(hItem);
		string path = Text::fromT(pItem->m_sFQPath);
		Util::appendPathSeparator(path);
		sm->removeExcludeFolder(path);
		if (listener) listener->onChange();
	}
}

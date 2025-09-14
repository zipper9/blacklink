/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"

#include <atlwin.h>
#include <shlobj.h>
#include <mmsystem.h>
#include <powrprof.h>

#include "WinUtil.h"
#include "LineDlg.h"

#include "../client/SimpleStringTokenizer.h"
#include "../client/ShareManager.h"
#include "../client/File.h"
#include "../client/DownloadManager.h"
#include "../client/QueueManager.h"
#include "../client/ParamExpander.h"
#include "../client/MagnetLink.h"
#include "../client/AppPaths.h"
#include "../client/PathUtil.h"
#include "../client/Util.h"
#include "../client/SysVersion.h"
#include "../client/SettingsUtil.h"
#include "../client/ConfCore.h"
#include "Colors.h"
#include "Fonts.h"
#include "MDIUtil.h"
#include "MagnetDlg.h"
#include "PreviewMenu.h"
#include "FlatTabCtrl.h"
#include "HubFrame.h"
#include "SearchFrm.h"
#include "DirectoryListingFrm.h"
#include "WinSysHandlers.h"
#include <boost/algorithm/string/trim.hpp>

const WinUtil::FileMaskItem WinUtil::fileListsMask[] =
{
	{ ResourceManager::FILEMASK_FILE_LISTS, _T("*.xml.bz2;*.dcls;*.dclst") },
	{ ResourceManager::FILEMASK_XML_BZ2,    _T("*.xml.bz2")                },
	{ ResourceManager::FILEMASK_DCLST,      _T("*.dcls;*.dclst")           },
	{ ResourceManager::FILEMASK_ALL,        _T("*.*")                      },
	{ ResourceManager::Strings(),           nullptr                        }
};

const WinUtil::FileMaskItem WinUtil::allFilesMask[] =
{
	{ ResourceManager::FILEMASK_ALL, _T("*.*") },
	{ ResourceManager::Strings(),    nullptr   }
};

TStringList LastDir::dirs;
HWND WinUtil::g_mainWnd = nullptr;
bool WinUtil::g_isAppActive = false;

const GUID WinUtil::guidGetTTH          = { 0xbc034ae0, 0x40d8, 0x465d, { 0xb1, 0xf0, 0x01, 0xd9, 0xd8, 0x83, 0x7f, 0x96 } };
const GUID WinUtil::guidDcLstFromFolder = { 0xbc034ae0, 0x40d8, 0x465d, { 0xb1, 0xf0, 0x01, 0xd9, 0xd8, 0x83, 0x7f, 0x97 } };

uint64_t WinUtil::getNewFrameID(int type)
{
	static uint64_t nextFrameId = 0;
	uint64_t id = ++nextFrameId;
	return id << 8 | type;
}

void WinUtil::escapeMenu(tstring& text)
{
	string::size_type i = 0;
	while ((i = text.find(_T('&'), i)) != string::npos)
	{
		text.insert(i, 1, _T('&'));
		i += 2;
	}
}

const tstring& WinUtil::escapeMenu(const tstring& text, tstring& tmp)
{
	string::size_type i = text.find(_T('&'));
	if (i == string::npos) return text;
	tmp = text;
	tmp.insert(i, 1, _T('&'));
	i += 2;
	while ((i = tmp.find(_T('&'), i)) != string::npos)
	{
		tmp.insert(i, 1, _T('&'));
		i += 2;
	}
	return tmp;
}

void WinUtil::appendSeparator(HMENU hMenu)
{
	CMenuHandle menu(hMenu);
	int count = menu.GetMenuItemCount();
	if (!count) return;
	UINT state = menu.GetMenuState(count-1, MF_BYPOSITION);
	if ((state & MF_POPUP) || !(state & MF_SEPARATOR))
		menu.AppendMenu(MF_SEPARATOR);
}

void WinUtil::appendSeparator(OMenu& menu)
{
	int count = menu.GetMenuItemCount();
	if (!count) return;
	UINT state = menu.GetMenuState(count-1, MF_BYPOSITION);
	if ((state & MF_POPUP) || !(state & MF_SEPARATOR))
		menu.AppendMenu(MF_SEPARATOR);
}

void WinUtil::init(HWND hWnd)
{
	g_mainWnd = hWnd;

	PreviewMenu::init();

	g_fileImage.init();
	g_videoImage.init();
	g_flagImage.init();
	g_userImage.init();
	g_userStateImage.init();
	g_genderImage.init();
	g_hubImage.init();
	g_fileListImage.init();
	g_otherImage.init();
	g_favUserImage.init();
	g_editorImage.init();
	g_transfersImage.init();
	g_transferArrowsImage.init();
	g_navigationImage.init();
	g_filterImage.init();

	Colors::init();
	Fonts::init();
	installKeyboardHook();

	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::REGISTER_URL_HANDLER))
		registerHubUrlHandlers();

	if (ss->getBool(Conf::REGISTER_MAGNET_HANDLER))
		registerMagnetHandler();

	if (ss->getBool(Conf::REGISTER_DCLST_HANDLER))
		registerDclstHandler();
}

void WinUtil::uninit()
{
	removeKeyboardHook();

	g_mainWnd = nullptr;

	g_fileImage.uninit();
	g_videoImage.uninit();
	g_userImage.uninit();
	g_userStateImage.uninit();
	g_genderImage.uninit();
	g_hubImage.uninit();
	g_fileListImage.uninit();
	g_otherImage.uninit();
	g_favUserImage.uninit();
	g_editorImage.uninit();
	g_TransferTreeImage.uninit();
	g_navigationImage.uninit();
	g_filterImage.uninit();

	Fonts::uninit();
	Colors::uninit();
}

void WinUtil::setClipboard(const tstring& str)
{
	if (!::OpenClipboard(g_mainWnd))
	{
		return;
	}
	
	EmptyClipboard();
	
	// Allocate a global memory object for the text.
	HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (str.size() + 1) * sizeof(TCHAR));
	if (hglbCopy == NULL)
	{
		CloseClipboard();
		return;
	}
	
	// Lock the handle and copy the text to the buffer.
	TCHAR* lptstrCopy = (TCHAR*)GlobalLock(hglbCopy);
	_tcscpy(lptstrCopy, str.c_str());
	GlobalUnlock(hglbCopy);
	
	// Place the handle on the clipboard.
	SetClipboardData(CF_UNICODETEXT, hglbCopy);
	
	CloseClipboard();
}

int WinUtil::splitTokensWidth(int* result, const string& tokens, int maxItems, int defaultValue) noexcept
{
	int count = splitTokens(result, tokens, maxItems);
	for (int k = 0; k < count; ++k)
		if (result[k] <= 0 || result[k] > 2000)
			result[k] = defaultValue;
	return count;
}

int WinUtil::splitTokens(int* result, const string& tokens, int maxItems) noexcept
{
	SimpleStringTokenizer<char> t(tokens, ',');
	string tok;
	int k = 0;
	while (k < maxItems && t.getNextToken(tok))
		result[k++] = Util::toInt(tok);
	return k;
}

bool WinUtil::getUCParams(HWND parent, const UserCommand& uc, StringMap& sm)
{
	string::size_type i = 0;
	StringSet done;
	
	while ((i = uc.getCommand().find("%[line:", i)) != string::npos)
	{
		i += 7;
		string::size_type j = uc.getCommand().find(']', i);
		if (j == string::npos)
			break;
			
		string name = uc.getCommand().substr(i, j - i);
		if (done.find(name) == done.end())
		{
			LineDlg dlg;
			dlg.title = Text::toT(Util::toString(" > ", uc.getDisplayName()));
			dlg.description = Text::toT(name);
			dlg.line = Text::toT(sm["line:" + name]);
			dlg.icon = uc.isSet(UserCommand::FLAG_NOSAVE) ? IconBitmaps::HUB_ONLINE : IconBitmaps::COMMANDS;

			if (uc.isSet(UserCommand::FLAG_FROM_ADC_HUB))
			{
				Util::replace(_T("\\\\"), _T("\\"), dlg.description);
				Util::replace(_T("\\s"), _T(" "), dlg.description);
			}

			if (dlg.DoModal(parent) != IDOK) return false;

			string str = Text::fromT(dlg.line);
			sm["line:" + name] = str;
			done.insert(name);
		}
		i = j + 1;
	}
	i = 0;
	while ((i = uc.getCommand().find("%[kickline:", i)) != string::npos)
	{
		i += 11;
		string::size_type j = uc.getCommand().find(']', i);
		if (j == string::npos)
			break;
			
		string name = uc.getCommand().substr(i, j - i);
		if (done.find(name) == done.end())
		{
			KickDlg dlg;
			dlg.title = Text::toT(Util::toString(" > ", uc.getDisplayName()));
			dlg.description = Text::toT(name);

			if (uc.isSet(UserCommand::FLAG_FROM_ADC_HUB))
			{
				Util::replace(_T("\\\\"), _T("\\"), dlg.description);
				Util::replace(_T("\\s"), _T(" "), dlg.description);
			}
			
			if (dlg.DoModal(parent) != IDOK) return false;

			string str = Text::fromT(dlg.line);
			sm["kickline:" + name] = str;
			done.insert(name);
		}
		i = j + 1;
	}
	return true;
}

void WinUtil::copyMagnet(const TTHValue& hash, const string& file, int64_t size)
{
	if (!file.empty())
		setClipboard(Text::toT(Util::getMagnet(hash, file, size)));
}

void WinUtil::searchFile(const string& file)
{
	SearchFrame::openWindow(Text::toT(file), 0, SIZE_DONTCARE, FILE_TYPE_ANY);
}

void WinUtil::searchHash(const TTHValue& hash)
{
	SearchFrame::openWindow(Text::toT(hash.toBase32()), 0, SIZE_DONTCARE, FILE_TYPE_TTH);
}

void WinUtil::playSound(const string& soundFile, bool beep /* = false */)
{
	if (!soundFile.empty())
		PlaySound(Text::toT(soundFile).c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
	else if (beep)
		MessageBeep(MB_OK);
}

void WinUtil::openFile(const tstring& file)
{
	openFile(file.c_str());
}

void WinUtil::openFile(const TCHAR* file)
{
	::ShellExecute(NULL, _T("open"), file, NULL, NULL, SW_SHOWNORMAL);
}

static void shellExecute(const tstring& url)
{
#ifdef FLYLINKDC_USE_TORRENT
	if (url.find(_T("magnet:?xt=urn:btih")) != tstring::npos)
	{
		DownloadManager::getInstance()->add_torrent_file(_T(""), url);
		return;
	}
#endif
	::ShellExecute(NULL, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

bool WinUtil::openLink(const tstring& uri)
{
	if (parseMagnetUri(uri) || parseDchubUrl(uri))
		return true;
	return openWebLink(uri);
}

bool WinUtil::openWebLink(const tstring& uri)
{
	static const tstring extLinks[] =
	{
		_T("http://"),
		_T("https://"),
		_T("ftp://"),
		_T("irc://"),
		_T("skype:"),
		_T("ed2k://"),
		_T("mms://"),
		_T("xmpp://"),
		_T("nfs://"),
		_T("mailto:"),
		_T("www.")
	};
	for (const tstring& link : extLinks)
	{
		if (strnicmp(uri, link, link.length()) == 0)
		{
			shellExecute(uri);
			return true;
		}
	}
	return false;
}

bool WinUtil::parseDchubUrl(const tstring& url)
{
	Util::ParsedUrl p;
	Util::decodeUrl(Text::fromT(url), p);
	int protocol = Util::getHubProtocol(p.protocol);
	if (!protocol || p.host.empty() || p.port == 0) return false;

	string nick = std::move(p.user);
	string file = Util::decodeUri(p.path);
	const string formattedUrl = Util::formatDchubUrl(p);

	RecentHubEntry r;
	r.setOpenTab("+");
	r.setServer(formattedUrl);
	FavoriteManager::getInstance()->addRecent(r);
	HubFrame::openHubWindow(formattedUrl);

	if (!file.empty())
	{
		if (nick.empty())
		{
			if (file[0] == '/') file.erase(0, 1);
			const string::size_type i = file.find('/');
			if (i != string::npos)
			{
				nick = file.substr(0, i);
				file.erase(0, i);
			}
			else
			{
				nick = std::move(file);
				file = "/";
			}
		}
		if (!file.empty() && file.back() != '/')
		{
			const string::size_type i = file.rfind('/');
			if (i != string::npos) file.erase(i + 1);
		}
		if (!nick.empty())
		{
			UserPtr user;
			if ((protocol == Util::HUB_PROTOCOL_ADC || protocol == Util::HUB_PROTOCOL_ADCS) && Util::isTigerHashString(nick))
				user = ClientManager::findUser(CID(nick));
			else
				user = ClientManager::findLegacyUser(nick, formattedUrl);
			if (user && !user->isMe() && ClientManager::isOnline(user))
			{
				try
				{
					QueueManager::getInstance()->addList(HintedUser(user, formattedUrl), 0, QueueItem::XFLAG_CLIENT_VIEW, file);
				}
				catch (const Exception&)
				{
					// Ignore for now...
				}
			}
		}
	}
	return true;
}

static WinUtil::DefinedMagnetAction getMagnetAction(int setting, bool alreadyShared)
{
	switch (setting)
	{
		case Conf::MAGNET_ACTION_DOWNLOAD:
			return alreadyShared ? WinUtil::MA_ASK : WinUtil::MA_DOWNLOAD;
		case Conf::MAGNET_ACTION_SEARCH:
			return WinUtil::MA_SEARCH;
		case Conf::MAGNET_ACTION_DOWNLOAD_AND_OPEN:
			return alreadyShared ? WinUtil::MA_ASK : WinUtil::MA_OPEN;
		case Conf::MAGNET_ACTION_OPEN_EXISTING:
			return alreadyShared ? WinUtil::MA_OPEN : WinUtil::MA_ASK;
	}
	return WinUtil::MA_ASK;
}

bool WinUtil::parseMagnetUri(const tstring& text, DefinedMagnetAction action /* = MA_DEFAULT */)
{
	if (Util::isMagnetLink(text))
	{
		const string url = Text::fromT(text);
		LogManager::message(STRING(MAGNET_DLG_TITLE) + ": " + url);
		MagnetLink magnet;
		const char* fhash;
		if (!magnet.parse(url) || (fhash = magnet.getTTH()) == nullptr)
		{
			MessageBox(g_mainWnd, CTSTRING(MAGNET_DLG_TEXT_BAD), CTSTRING(MAGNET_DLG_TITLE), MB_OK | MB_ICONEXCLAMATION);
			return false;
		}
		string fname = magnet.getFileName();
		TTHValue tth(fhash);
		string localPath;
		ShareManager::getInstance()->getFileInfo(tth, localPath);

		if (localPath.empty() && !magnet.exactSource.empty())
			WinUtil::parseDchubUrl(Text::toT(magnet.exactSource));

		const bool isDclst = Util::isDclstFile(fname);
		const auto* ss = SettingsManager::instance.getUiSettings();
		if (action == MA_DEFAULT)
		{
			if (!localPath.empty())
				action = ss->getBool(Conf::SHARED_MAGNET_ASK) ?
					MA_ASK : getMagnetAction(ss->getInt(Conf::SHARED_MAGNET_ACTION), true);
			else if (!isDclst)
				action = ss->getBool(Conf::MAGNET_ASK) ?
					MA_ASK : getMagnetAction(ss->getInt(Conf::MAGNET_ACTION), false);
			else
				action = ss->getBool(Conf::DCLST_ASK) ?
					MA_ASK : getMagnetAction(ss->getInt(Conf::DCLST_ACTION), false);
		}
		if (action == MA_ASK)
		{
			tstring filename = Text::toT(fname);
			int flags = 0;
			if (isDclst) flags |= MagnetDlg::FLAG_DCLST;
			if (!localPath.empty()) flags |= MagnetDlg::FLAG_ALREADY_SHARED;
			action = MagnetDlg::showDialog(g_mainWnd, flags, tth, filename, magnet.exactLength, magnet.dirSize);
			if (action == WinUtil::MA_DEFAULT) return true;
			fname = Text::fromT(filename);
		}
		switch (action)
		{
			case MA_DOWNLOAD:
			case MA_OPEN:
				if (!localPath.empty())
				{
					if (isDclst)
						openFileList(Text::toT(localPath));
					else
						openFile(Text::toT(localPath));
					break;
				}
				try
				{
					bool getConnFlag = true;
					QueueItem::MaskType flags = isDclst ? QueueItem::FLAG_DCLST_LIST : 0;
					QueueItem::MaskType extraFlags = 0;
					if (action == MA_OPEN)
						flags |= QueueItem::XFLAG_CLIENT_VIEW;
					else if (isDclst)
						flags |= QueueItem::XFLAG_DOWNLOAD_CONTENTS;
					QueueManager::QueueItemParams params;
					params.size = magnet.exactLength;
					params.root = &tth;
					QueueManager::getInstance()->add(fname, params, HintedUser(), flags, extraFlags, getConnFlag);
					if (ss->getBool(Conf::SEARCH_MAGNET_SOURCES))
						SearchFrame::openWindow(Text::toT(fhash), 0, SIZE_DONTCARE, FILE_TYPE_TTH);
				}
				catch (const Exception& e)
				{
					LogManager::message(e.getError());
				}
				break;
			case MA_SEARCH:
				SearchFrame::openWindow(Text::toT(fhash), 0, SIZE_DONTCARE, FILE_TYPE_TTH);
				break;
		}
		return true;
	}
	return false;
}

void WinUtil::openFileList(const tstring& filename)
{
	const UserPtr u = DirectoryListing::getUserFromFilename(Text::fromT(filename));
	DirectoryListingFrame::openWindow(filename, Util::emptyStringT, HintedUser(u, Util::emptyString), 0, Util::isDclstFile(Text::fromT(filename)));
}

// !SMT!-UI (todo: disable - this routine does not save column visibility)
void WinUtil::saveHeaderOrder(CListViewCtrl& ctrl, int order, int widths, int n,
                              int* indexes, int* sizes) noexcept
{
	auto ss = SettingsManager::instance.getUiSettings();
	string tmp;

	ctrl.GetColumnOrderArray(n, indexes);
	int i;
	for (i = 0; i < n; ++i)
	{
		tmp += Util::toString(indexes[i]);
		tmp += ',';
	}
	tmp.erase(tmp.size() - 1, 1);
	ss->setString(order, tmp);
	tmp.clear();
	const int nHeaderItemsCount = ctrl.GetHeader().GetItemCount();
	for (i = 0; i < n; ++i)
	{
		sizes[i] = ctrl.GetColumnWidth(i);
		if (i >= nHeaderItemsCount) // Invalid index
			sizes[i] = 0;
		tmp += Util::toString(sizes[i]);
		tmp += ',';
	}
	tmp.erase(tmp.size() - 1, 1);
	ss->setString(widths, tmp);
}

static pair<tstring, bool> formatHubNames(const StringList& hubs)
{
	if (hubs.empty())
	{
		return pair<tstring, bool>(TSTRING(OFFLINE), false);
	}
	else
	{
		return pair<tstring, bool>(Text::toT(Util::toString(hubs)), true);
	}
}

pair<tstring, bool> WinUtil::getHubNames(const CID& cid, const string& hintUrl)
{
	return formatHubNames(ClientManager::getHubNames(cid, hintUrl));
}

pair<tstring, bool> WinUtil::getHubNames(const UserPtr& u, const string& hintUrl)
{
	return getHubNames(u->getCID(), hintUrl);
}

pair<tstring, bool> WinUtil::getHubNames(const CID& cid, const string& hintUrl, bool priv)
{
	return formatHubNames(ClientManager::getHubNames(cid, hintUrl, priv));
}

pair<tstring, bool> WinUtil::getHubNames(const HintedUser& user)
{
	return getHubNames(user.user, user.hint);
}

string WinUtil::getHubDisplayName(const string& hubUrl)
{
	if (hubUrl == "DHT") return hubUrl;
	auto fm = FavoriteManager::getInstance();
	const FavoriteHubEntry* fhe = fm->getFavoriteHubEntryPtr(hubUrl);
	if (fhe)
	{
		string name = fhe->getName();
		fm->releaseFavoriteHubEntryPtr(fhe);
		return name;
	}
	string name = ClientManager::getOnlineHubName(hubUrl);
	return name.empty() ? hubUrl : name;
}

void WinUtil::getContextMenuPos(const CListViewCtrl& list, POINT& pt)
{
	int pos = list.GetNextItem(-1, LVNI_SELECTED | LVNI_FOCUSED);
	if (pos >= 0)
	{
		CRect rc;
		list.GetItemRect(pos, &rc, LVIR_LABEL);
		pt.x = 16;
		pt.y = (rc.top + rc.bottom) / 2;
	}
	else
		pt.x = pt.y = 0;
	list.ClientToScreen(&pt);
}

void WinUtil::getContextMenuPos(const CTreeViewCtrl& tree, POINT& pt)
{
	HTREEITEM ht = tree.GetSelectedItem();
	if (ht)
	{
		CRect rc;
		tree.GetItemRect(ht, &rc, TRUE);
		pt.x = std::max<int>(rc.left, 16);
		pt.y = (rc.top + rc.bottom) / 2;
	}
	else
		pt.x = pt.y = 0;
	tree.ClientToScreen(&pt);
}

void WinUtil::getContextMenuPos(const CEdit& edit, POINT& pt)
{
	CRect rc;
	edit.GetRect(&rc);
	pt.x = rc.Width() / 2;
	pt.y = rc.Height() / 2;
	edit.ClientToScreen(&pt);
}

void WinUtil::openFolder(const tstring& file)
{
	if (File::isExist(file))
		::ShellExecute(NULL, NULL, _T("explorer.exe"), (_T("/e, /select, \"") + file + _T('\"')).c_str(), NULL, SW_SHOWNORMAL);
	else
		::ShellExecute(NULL, NULL, _T("explorer.exe"), (_T("/e, \"") + Util::getFilePath(file) + _T('\"')).c_str(), NULL, SW_SHOWNORMAL);
}

void WinUtil::openLog(const string& dir, const StringMap& params, const tstring& noLogMessage)
{
	const auto file = Text::toT(Util::validateFileName(Util::getConfString(Conf::LOG_DIRECTORY) + Util::formatParams(dir, params, true)));
	if (File::isExist(file))
		WinUtil::openFile(file);
	else
		MessageBox(nullptr, noLogMessage.c_str(), getAppNameVerT().c_str(), MB_OK | MB_ICONINFORMATION);
}

bool WinUtil::shutDown(int action)
{
	// Prepare for shutdown
	static const UINT forceIfHung = 0x00000010;
	HANDLE hToken;
	OpenProcessToken(GetCurrentProcess(), (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY), &hToken);

	LUID luid;
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid);

	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tp.Privileges[0].Luid = luid;
	AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);
	CloseHandle(hToken);

	// Shutdown
	switch (action)
	{
		case 0:
		{
			action = EWX_POWEROFF;
			break;
		}
		case 1:
		{
			action = EWX_LOGOFF;
			break;
		}
		case 2:
		{
			action = EWX_REBOOT;
			break;
		}
		case 3:
		{
			SetSuspendState(false, false, false);
			return true;
		}
		case 4:
		{
			SetSuspendState(true, false, false);
			return true;
		}
		case 5:
		{
			typedef BOOL (CALLBACK *fnLockWorkStation)();
			HMODULE hModule = GetModuleHandle(_T("user32"));
			if (hModule)
			{
				fnLockWorkStation ptrLockWorkStation = (fnLockWorkStation) GetProcAddress(hModule, "LockWorkStation");
				if (ptrLockWorkStation) return ptrLockWorkStation() != FALSE;
			}
			return false;
		}
	}
	return ExitWindowsEx(action | forceIfHung, 0) != 0;
}

void WinUtil::getAdapterList(int af, vector<Util::AdapterInfo>& adapters, int options)
{
	adapters.clear();
	Util::getNetworkAdapters(af, adapters, options);
	IpAddressEx defaultAdapter;
	memset(&defaultAdapter, 0, sizeof(defaultAdapter));
	defaultAdapter.type = af;
	if (std::find_if(adapters.cbegin(), adapters.cend(),
		[&defaultAdapter](const auto &v) { return v.ip == defaultAdapter; }) == adapters.cend())
	{
		adapters.insert(adapters.begin(), Util::AdapterInfo(Util::emptyString, TSTRING(DEFAULT_ADAPTER), defaultAdapter, 0, 0));
	}
}

int WinUtil::fillAdapterList(int af, const vector<Util::AdapterInfo>& adapters, CComboBox& bindCombo, const string& selected, int options)
{
	int selIndex = -1;
	for (size_t i = 0; i < adapters.size(); ++i)
	{
		string address = Util::printIpAddress(adapters[i].ip);
		tstring text = Text::toT(address);
		if (!adapters[i].description.empty())
		{
			text += _T(" (");
			text += adapters[i].description;
			text += _T(')');
		}
		bindCombo.AddString(text.c_str());
		if (options & Conf::BIND_OPTION_USE_DEV)
		{
			if (adapters[i].name == selected) selIndex = i;
		}
		else
		{
			if (address == selected) selIndex = i;
		}
	}
	if (selected.empty())
		selIndex = 0;
	int result = selIndex;
	if (selIndex == -1)
	{
		if (options & Conf::BIND_OPTION_USE_DEV)
			selIndex = 0;
		else
			selIndex = bindCombo.InsertString(-1, Text::toT(selected).c_str());
	}
	bindCombo.SetCurSel(selIndex);
	return result;
}

string WinUtil::getSelectedAdapter(const CComboBox& bindCombo)
{
	tstring ts;
	WinUtil::getWindowText(bindCombo, ts);
	string str = Text::fromT(ts);
	boost::trim(str);
	string::size_type pos = str.find(' ');
	if (pos != string::npos) str.erase(pos);
	return str;
}

void WinUtil::fillCharsetList(CComboBox& comboBox, int selected, bool onlyUTF8, bool inFavs)
{
	static const ResourceManager::Strings charsetNames[Text::NUM_SUPPORTED_CHARSETS] =
	{
		ResourceManager::ENCODING_CP1250,
		ResourceManager::ENCODING_CP1251,
		ResourceManager::ENCODING_CP1252,
		ResourceManager::ENCODING_CP1253,
		ResourceManager::ENCODING_CP1254,
		ResourceManager::ENCODING_CP1255,
		ResourceManager::ENCODING_CP1256,
		ResourceManager::ENCODING_CP1257,
		ResourceManager::ENCODING_CP1258,
		ResourceManager::ENCODING_CP936,
		ResourceManager::ENCODING_CP950
	};

	int index, selIndex = -1;
	if (!onlyUTF8)
	{
		tstring str;
		if (inFavs)
		{
			int defaultCharset = Text::charsetFromString(Util::getConfString(Conf::DEFAULT_CODEPAGE));
			if (defaultCharset == Text::CHARSET_SYSTEM_DEFAULT) defaultCharset = Text::getDefaultCharset();
			str = TSTRING_F(ENCODING_DEFAULT, defaultCharset);
		}
		else
			str = TSTRING_F(ENCODING_SYSTEM_DEFAULT, Text::getDefaultCharset());
		index = comboBox.AddString(str.c_str());
		if (selected == Text::CHARSET_SYSTEM_DEFAULT) selIndex = index;
		comboBox.SetItemData(index, Text::CHARSET_SYSTEM_DEFAULT);
	}
	index = comboBox.AddString(CTSTRING(ENCODING_UTF8));
	if (selected == Text::CHARSET_UTF8) selIndex = index;
	comboBox.SetItemData(index, Text::CHARSET_UTF8);
	if (!onlyUTF8)
		for (int i = 0; i < Text::NUM_SUPPORTED_CHARSETS; i++)
		{
			int charset = Text::supportedCharsets[i];
			tstring str = TSTRING_I(charsetNames[i]);
			str += _T(" (");
			str += Util::toStringT(charset);
			str += _T(')');
			index = comboBox.AddString(str.c_str());
			if (selected == charset) selIndex = index;
			comboBox.SetItemData(index, charset);
		}

	if (selIndex < 0 || onlyUTF8) selIndex = 0;
	comboBox.SetCurSel(selIndex);
}

int WinUtil::getSelectedCharset(const CComboBox& comboBox)
{
	int selIndex = comboBox.GetCurSel();
	return comboBox.GetItemData(selIndex);
}

void WinUtil::fillTimeValues(CComboBox& comboBox)
{
	tm t;
	memset(&t, 0, sizeof(t));
	TCHAR buf[64];
	tstring ts;
	comboBox.AddString(CTSTRING(MIDNIGHT));
	for (int i = 1; i < 24; ++i)
	{
		if (i == 12)
		{
			comboBox.AddString(CTSTRING(NOON));
			continue;
		}
		t.tm_hour = i;
		_tcsftime(buf, _countof(buf), _T("%X"), &t);
		ts.assign(buf);
		auto pos = ts.find(_T(":00:00"));
		if (pos != tstring::npos) ts.erase(pos + 3, 3);
		comboBox.AddString(ts.c_str());
	}
}

void WinUtil::addTool(CToolTipCtrl& ctrl, HWND hWnd, ResourceManager::Strings str)
{
	CToolInfo ti(TTF_SUBCLASS, hWnd, 0, nullptr, const_cast<LPTSTR>(CTSTRING_I(str)));
	ATLVERIFY(ctrl.AddTool(&ti));
}

#ifdef SSA_SHELL_INTEGRATION
tstring WinUtil::getShellExtDllPath()
{
	static const auto filePath = Text::toT(Util::getExePath()) + _T("BLShellExt")
#if defined(_WIN64)
	                             _T("_x64")
#endif
	                             _T(".dll");

	return filePath;
}

bool WinUtil::registerShellExt(bool unregister)
{
	typedef HRESULT (WINAPIV *Registration)();
	
	bool result = false;
	HINSTANCE hModule = nullptr;
	try
	{
		const auto filePath = WinUtil::getShellExtDllPath();
		hModule = LoadLibrary(filePath.c_str());
		if (hModule != nullptr)
		{
			Registration reg = nullptr;
			reg = (Registration) GetProcAddress((HMODULE)hModule, unregister ? "DllUnregisterServer" : "DllRegisterServer");
			if (reg)
				result = SUCCEEDED(reg());
			FreeLibrary(hModule);
		}
	}
	catch (...)
	{
		if (hModule)
			FreeLibrary(hModule);
	}
	return result;
}
#endif // SSA_SHELL_INTEGRATION

bool WinUtil::runElevated(HWND hwnd, const TCHAR* path, const TCHAR* parameters, const TCHAR* directory, int waitTime)
{
	SHELLEXECUTEINFO shex = { sizeof(SHELLEXECUTEINFO) };
	shex.fMask        = waitTime ? SEE_MASK_NOCLOSEPROCESS : 0;
	shex.hwnd         = hwnd;
	shex.lpVerb       = _T("runas");
	shex.lpFile       = path;
	shex.lpParameters = parameters;
	shex.lpDirectory  = directory;
	shex.nShow        = SW_NORMAL;

	if (!ShellExecuteEx(&shex)) return false;
	if (!waitTime) return true;
	bool result = WaitForSingleObject(shex.hProcess, waitTime) == WAIT_OBJECT_0;
	CloseHandle(shex.hProcess);
	return result;
}

bool WinUtil::createShortcut(const tstring& targetFile, const tstring& targetArgs,
                             const tstring& linkFile, const tstring& description,
                             int showMode, const tstring& workDir,
                             const tstring& iconFile, int iconIndex)
{
	if (targetFile.empty() || linkFile.empty()) return false;

	bool result = false;
	IShellLink*   pShellLink = nullptr;
	IPersistFile* pPersistFile = nullptr;

	do
	{
		if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**) &pShellLink))
			|| !pShellLink) break;

		if (FAILED(pShellLink->SetPath(targetFile.c_str()))) break;
		if (FAILED(pShellLink->SetArguments(targetArgs.c_str()))) break;
		if (!description.empty() && FAILED(pShellLink->SetDescription(description.c_str()))) break;
		if (showMode > 0 && FAILED(pShellLink->SetShowCmd(showMode))) break;
		if (!workDir.empty() && FAILED(pShellLink->SetWorkingDirectory(workDir.c_str()))) break;
		if (!iconFile.empty() && iconIndex >= 0 && FAILED(pShellLink->SetIconLocation(iconFile.c_str(), iconIndex))) break;

		if (FAILED(pShellLink->QueryInterface(IID_IPersistFile, (void**) &pPersistFile))
			|| !pPersistFile) break;
		if (FAILED(pPersistFile->Save(linkFile.c_str(), TRUE))) break;

		result = true;	
	} while (0);
	
	if (pPersistFile)
		pPersistFile->Release();
	if (pShellLink)
		pShellLink->Release();
	
	return result;
}

bool WinUtil::autoRunShortcut(bool create)
{
	tstring linkFile = getAutoRunShortcutName();
	if (create)
	{
		if (!File::isExist(linkFile))
		{
			tstring targetFile = Util::getModuleFileName();
			tstring workDir = Util::getFilePath(targetFile);
			tstring description = getAppNameVerT();
			Util::appendPathSeparator(workDir);
			return createShortcut(targetFile, _T(""), linkFile, description, 0, workDir, targetFile, 0);
		}
	}
	else
	{
		if (File::isExist(linkFile))
			return File::deleteFile(linkFile);
	}	
	return true;
}

bool WinUtil::isAutoRunShortcutExists()
{
	return File::isExist(getAutoRunShortcutName());
}

tstring WinUtil::getAutoRunShortcutName()
{
	TCHAR startupPath[MAX_PATH];
	if (!SHGetSpecialFolderPath(NULL, startupPath, CSIDL_STARTUP, TRUE))
		return Util::emptyStringT;

	tstring result = startupPath;
	Util::appendPathSeparator(result);
	result += getAppNameT();
#if defined(_WIN64)
	result += _T("_x64");
#endif
	result += _T(".lnk");
	
	return result;
}

int WinUtil::getTextWidth(const tstring& str, HWND hWnd)
{
	int width = 0;
	if (str.length())
	{
		HFONT hFont = (HFONT) SendMessage(hWnd, WM_GETFONT, 0, 0);
		HDC dc = GetDC(hWnd);
		if (dc)
		{
			HGDIOBJ prevObj = hFont ? SelectObject(dc, hFont) : nullptr;
			SIZE size;
			GetTextExtentPoint32(dc, str.c_str(), str.length(), &size);
			width = size.cx;
			if (prevObj) SelectObject(dc, prevObj);
			ReleaseDC(hWnd, dc);
		}
	}
	return width;
}

int WinUtil::getTextWidth(const tstring& str, HDC dc)
{
	SIZE sz = { 0, 0 };
	if (str.length())
		GetTextExtentPoint32(dc, str.c_str(), str.length(), &sz);
	return sz.cx;
}

int WinUtil::getTextHeight(HDC dc)
{
	TEXTMETRIC tm = {};
	GetTextMetrics(dc, &tm);
	return tm.tmHeight;
}

int WinUtil::getTextHeight(HDC dc, HFONT hFont)
{
	HGDIOBJ prevObj = SelectObject(dc, hFont);
	int h = getTextHeight(dc);
	SelectObject(dc, prevObj);
	return h;
}

int WinUtil::getTextHeight(HWND hWnd, HFONT hFont)
{
	HDC dc = GetDC(hWnd);
	int h = getTextHeight(dc, hFont);
	ReleaseDC(hWnd, dc);
	return h;
}

void WinUtil::getWindowText(HWND hwnd, tstring& text)
{
	int len = GetWindowTextLength(hwnd);
	if (len <= 0)
	{
		text.clear();
		return;
	}
	text.resize(len + 1);
	len = GetWindowText(hwnd, &text[0], len + 1);
	text.resize(len);
}

tstring WinUtil::getComboBoxItemText(HWND hwnd, int index)
{
	int len = SendMessage(hwnd, CB_GETLBTEXTLEN, index, 0);
	tstring res;
	if (len > 0)
	{
		res.resize(len + 1);
		TCHAR* buf = &res[0];
		SendMessage(hwnd, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(buf));
		res.resize(len);
	}
	return res;
}

void WinUtil::fillComboBoxStrings(HWND hwnd, const ResourceManager::Strings* strings)
{
	for (int i = 0; strings[i] != R_INVALID; ++i)
	{
		const TCHAR* s = CTSTRING_I(strings[i]);
		SendMessage(hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
	}
}

void WinUtil::fillComboBoxStrings(HWND hwnd, const ResourceManager::Strings* strings, int count)
{
	for (int i = 0; i < count; ++i)
	{
		const TCHAR* s = CTSTRING_I(strings[i]);
		SendMessage(hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
	}
}

bool WinUtil::setExplorerTheme(HWND hWnd)
{
#ifdef OSVER_WIN_XP
	if (!SysVersion::isOsVistaPlus()) return false;
#endif
	if (!IsAppThemed()) return false;
	SetWindowTheme(hWnd, L"explorer", nullptr);
	return true;
}

static bool hasTreeViewDarkMode(HWND hWnd)
{
	static int treeViewDarkModeFlag = -1;
	if (treeViewDarkModeFlag != -1) return treeViewDarkModeFlag != 0;
	ThemeWrapper theme;
	bool result = theme.openTheme(hWnd, L"DarkMode_Explorer::TreeView");
	treeViewDarkModeFlag = result ? 1 : 0;
	return result;
}

bool WinUtil::setTreeViewTheme(HWND hWnd, bool darkMode)
{
#ifdef OSVER_WIN_XP
	if (!SysVersion::isOsVistaPlus()) return false;
#endif
	if (!IsAppThemed()) return false;
	if (darkMode && !hasTreeViewDarkMode(hWnd)) darkMode = false;
	SetWindowTheme(hWnd, darkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);
	return true;
}

unsigned WinUtil::getListViewExStyle(bool checkboxes)
{
	unsigned style = LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP;
	if (checkboxes)
		style |= LVS_EX_CHECKBOXES;
	if (SettingsManager::instance.getUiSettings()->getBool(Conf::SHOW_GRIDLINES))
		style |= LVS_EX_GRIDLINES;
	return style;
}

unsigned WinUtil::getTreeViewStyle()
{
	return TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP | TVS_HASBUTTONS | TVS_LINESATROOT;
}

bool WinUtil::getDialogUnits(HWND hwnd, HFONT font, int& cx, int& cy)
{
	if (!font)
	{
		font = (HFONT) ::SendMessage(hwnd, WM_GETFONT, 0, 0);
		if (!font) font = Fonts::g_systemFont;
	}
	HDC hdc = GetDC(hwnd);
	if (!hdc) return false;
	HGDIOBJ prevFont = SelectObject(hdc, font);
	bool res = getDialogUnits(hdc, cx, cy);
	SelectObject(hdc, prevFont);
	ReleaseDC(hwnd, hdc);
	return res;
}

bool WinUtil::getDialogUnits(HDC hdc, int& cx, int& cy)
{
	TEXTMETRIC tm;
	if (GetTextMetrics(hdc, &tm))
	{
		TCHAR ch[52];
		for (int i = 0; i < 26; i++)
		{
			ch[i] = 'a' + i;
			ch[26 + i] = 'A' + i;
		}
		SIZE size;
		if (GetTextExtentPoint(hdc, ch, 52, &size))
		{
			cx = (size.cx / 26 + 1) / 2;
			cy = tm.tmHeight;
			return true;
		}
	}
	return false;
}

int WinUtil::getComboBoxHeight(HWND hwnd, HFONT font)
{
	if (!font)
	{
		font = (HFONT) ::SendMessage(hwnd, WM_GETFONT, 0, 0);
		if (!font) font = Fonts::g_systemFont;
	}
	HDC hdc = GetDC(hwnd);
	if (!hdc) return 0;
	int res = 0;
	HGDIOBJ prevFont = SelectObject(hdc, font);
	SIZE size;
	TCHAR ch = _T('0');
	if (GetTextExtentPoint(hdc, &ch, 1, &size))
		res = size.cy + GetSystemMetrics(SM_CYEDGE) + 2*GetSystemMetrics(SM_CYFIXEDFRAME);
	SelectObject(hdc, prevFont);
	ReleaseDC(hwnd, hdc);
	return res;
}

void WinUtil::showInputError(HWND hwndCtl, const tstring& text)
{
	EDITBALLOONTIP ebt;
	memset(&ebt, 0, sizeof(ebt));
	ebt.cbStruct = sizeof(ebt);
	ebt.pszText = text.c_str();
	CEdit(hwndCtl).ShowBalloonTip(&ebt);
}

tstring WinUtil::getFileMaskString(const FileMaskItem* items)
{
	tstring result;
	while (items->ext)
	{
		result += TSTRING_I(items->stringId);
		result += _T('\0');
		result += items->ext;
		result += _T('\0');
		items++;
	}
	if (result.empty()) result += _T('\0');
	return result;
}

int WinUtil::getDllPlatform(const string& fullpath)
{
	#pragma pack(1)
	struct PE_START
	{
		uint32_t signature;
		uint16_t machine;
	};
	#pragma pack()

	WORD result = IMAGE_FILE_MACHINE_UNKNOWN;
	try
	{
		File f(fullpath, File::READ, File::OPEN);
		IMAGE_DOS_HEADER mz;
		size_t len = sizeof(mz);
		f.read(&mz, len);
		if (len == sizeof(mz) && mz.e_magic == IMAGE_DOS_SIGNATURE)
		{
			f.setPos(mz.e_lfanew);
			PE_START pe;
			len = sizeof(pe);
			f.read(&pe, len);
			if (len == sizeof(pe) && pe.signature == IMAGE_NT_SIGNATURE)
				result = pe.machine;
		}
	}
	catch (Exception&) {}
	return result;
}

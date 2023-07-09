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

#ifndef WIN_UTIL_H
#define WIN_UTIL_H

#include "resource.h"
#include "../client/Util.h"
#include "../client/NetworkUtil.h"
#include "../client/SettingsManager.h"
#include "UserInfoSimple.h"
#include "OMenu.h"
#include "HIconWrapper.h"
#include "ImageLists.h"

#define SHOW_POPUP(popup_key, msg, title) \
do \
{ \
	if (POPUP_ENABLED(popup_key)) \
		MainFrame::ShowBalloonTip(msg, title); \
} while(0)

#define SHOW_POPUPF(popup_key, msg, title, flags) \
do \
{ \
	if (POPUP_ENABLED(popup_key)) \
		MainFrame::ShowBalloonTip(msg, title, flags); \
} while(0)

#define SHOW_POPUP_EXT(popup_key, msg, ext_msg, ext_len, title) \
do \
{ \
	if (POPUP_ENABLED(popup_key)) \
		MainFrame::ShowBalloonTip(msg + _T(": ") + ext_msg.substr(0, ext_len), title); \
} while(0)

#define PLAY_SOUND(soundKey) WinUtil::playSound(SOUND_SETTING(soundKey))
#define PLAY_SOUND_BEEP(soundKey) { if (SOUND_BEEP_BOOLSETTING(soundKey)) WinUtil::playSound(SOUND_SETTING(SOUND_BEEPFILE), true); }

class FlatTabCtrl;
class UserCommand;

namespace WinUtil
{
	enum
	{
		MASK_FRAME_TYPE = 0xFF,
		FRAME_TYPE_MAIN = 1,
		FRAME_TYPE_HUB,
		FRAME_TYPE_PM,
		FRAME_TYPE_SEARCH,
		FRAME_TYPE_DIRECTORY_LISTING
	};

	struct TextItem
	{
		WORD itemID;
		ResourceManager::Strings translatedString;
	};

	uint64_t getNewFrameID(int type);

	extern HWND g_mainWnd;
	extern HWND g_mdiClient;
	extern FlatTabCtrl* g_tabCtrl;
	extern HHOOK g_hook;
	extern bool g_isAppActive;

	void init(HWND hWnd);
	void uninit();

	void escapeMenu(tstring& text);
	const tstring& escapeMenu(const tstring& text, tstring& tmp);

	void appendSeparator(HMENU menu);
	void appendSeparator(OMenu& menu);

	int getTextWidth(const tstring& str, HWND hWnd);
	int getTextWidth(const tstring& str, HDC dc);
	int getTextHeight(HDC dc);
	int getTextHeight(HDC dc, HFONT hFont);
	int getTextHeight(HWND hWnd, HFONT hFont);

	int getComboBoxHeight(HWND hwnd, HFONT font);
	bool getDialogUnits(HWND hwnd, HFONT font, int& cx, int& cy);
	bool getDialogUnits(HDC hdc, int& cx, int& cy);

	inline int dialogUnitsToPixelsX(int x, int xdu)
	{
		return (x * xdu + 2) / 4;
	}

	inline int dialogUnitsToPixelsY(int y, int ydu)
	{
		return (y * ydu + 4) / 8;
	}

	void showInputError(HWND hwndCtl, const tstring& text);

	void setClipboard(const tstring& str);
	inline void setClipboard(const string& str) { setClipboard(Text::toT(str)); }

	void copyMagnet(const TTHValue& hash, const string& file, int64_t size);

	void searchHash(const TTHValue& hash);
	void searchFile(const string& file);

	enum DefinedMagnetAction { MA_DEFAULT, MA_ASK, MA_DOWNLOAD, MA_SEARCH, MA_OPEN };

	bool parseDchubUrl(const tstring& url);
	bool parseMagnetUri(const tstring& url, DefinedMagnetAction action = MA_DEFAULT);

	void playSound(const string& soundFile, bool beep = false);

	void openFileList(const tstring& filename);
	bool openLink(const tstring& url);
	bool openWebLink(const tstring& url);
	void openFile(const tstring& file);
	void openFile(const TCHAR* file);
	void openLog(const string& dir, const StringMap& params, const tstring& noLogMessage);

	void openFolder(const tstring& file);

	// returns the position where the context menu should be
	// opened if it was invoked from the keyboard.
	// pt is relative to the screen not the control.
	void getContextMenuPos(const CListViewCtrl& list, POINT& pt);
	void getContextMenuPos(const CTreeViewCtrl& tree, POINT& pt);
	void getContextMenuPos(const CEdit& edit, POINT& pt);

	bool getUCParams(HWND parent, const UserCommand& cmd, StringMap& sm);

	// return pair of hubnames as a string and a bool representing the user's online status
	pair<tstring, bool> getHubNames(const CID& cid, const string& hintUrl);
	pair<tstring, bool> getHubNames(const UserPtr& u, const string& hintUrl);
	pair<tstring, bool> getHubNames(const CID& cid, const string& hintUrl, bool priv);
	pair<tstring, bool> getHubNames(const HintedUser& user);
	string getHubDisplayName(const string& hunUrl);
	int splitTokens(int* result, const string& tokens, int maxItems) noexcept;
	int splitTokensWidth(int* result, const string& tokens, int maxItems, int defaultValue = 100) noexcept;
	void saveHeaderOrder(CListViewCtrl& ctrl, SettingsManager::StrSetting order,
	                     SettingsManager::StrSetting widths, int n, int* indexes, int* sizes) noexcept; // !SMT!-UI todo: disable - this routine does not save column visibility

	inline bool isShift() { return (GetKeyState(VK_SHIFT) & 0x8000) != 0; }
	inline bool isAlt() { return (GetKeyState(VK_MENU) & 0x8000) != 0; }
	inline bool isCtrl() { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }

	template<class T> static HWND hiddenCreateEx(T& p) noexcept
	{
		const HWND active = (HWND)::SendMessage(g_mdiClient, WM_MDIGETACTIVE, 0, 0);
		LockWindowUpdate(g_mdiClient);
		HWND ret = p.CreateEx(g_mdiClient);
		if (active && ::IsWindow(active))
			::SendMessage(g_mdiClient, WM_MDIACTIVATE, (WPARAM)active, 0);
		LockWindowUpdate(NULL);
		return ret;
	}

	template<class T> static HWND hiddenCreateEx(T* p) noexcept
	{
		return hiddenCreateEx(*p);
	}

	static void translate(HWND page, const TextItem* textItems)
	{
		if (!textItems) return;
		for (size_t i = 0; textItems[i].itemID != 0; i++)
			::SetDlgItemText(page, textItems[i].itemID, CTSTRING_I(textItems[i].translatedString));
	}

	bool shutDown(int action);
	void activateMDIChild(HWND hWnd);

	tstring getNicks(const CID& cid, const string& hintUrl);
	tstring getNicks(const UserPtr& u, const string& hintUrl);
	tstring getNicks(const CID& cid, const string& hintUrl, bool priv);
	tstring getNicks(const HintedUser& user);

	bool setExplorerTheme(HWND hWnd);
	unsigned getListViewExStyle(bool checkboxes);
	unsigned getTreeViewStyle();
	void getAdapterList(int af, vector<Util::AdapterInfo>& adapters, int options = 0);
	int fillAdapterList(int af, const vector<Util::AdapterInfo>& adapters, CComboBox& bindCombo, const string& selected, int options);
	string getSelectedAdapter(const CComboBox& bindCombo);

	void fillCharsetList(CComboBox& comboBox, int selected, bool onlyUTF8, bool inFavs);
	int getSelectedCharset(const CComboBox& comboBox);

	void fillTimeValues(CComboBox& comboBox);
	void addTool(CToolTipCtrl& ctrl, HWND hWnd, ResourceManager::Strings str);

#ifdef SSA_SHELL_INTEGRATION
	tstring getShellExtDllPath();
	bool registerShellExt(bool unregister);
#endif
	bool runElevated(HWND hwnd, const TCHAR* path, const TCHAR* parameters = nullptr, const TCHAR* directory = nullptr, int waitTime = 0);

	bool autoRunShortcut(bool create);
	bool isAutoRunShortcutExists();
	tstring getAutoRunShortcutName();
	bool createShortcut(const tstring& targetFile, const tstring& targetArgs, const tstring& linkFile, const tstring& description, int showMode, const tstring& workDir, const tstring& iconFile, int iconIndex);

	void getWindowText(HWND hwnd, tstring& text);
	tstring getComboBoxItemText(HWND hwnd, int index);

	inline void limitStringLength(tstring& str, size_t maxLen = 40)
	{
		dcassert(maxLen > 3);
		if (str.length() > maxLen)
		{
			str.erase(maxLen - 3);
			str += _T("...");
		}
	}

	template<typename T>
	bool postSpeakerMsg(HWND hwnd, WPARAM wparam, T* ptr)
	{
		if (PostMessage(hwnd, WM_SPEAKER, wparam, reinterpret_cast<LPARAM>(ptr)))
			return true;
		delete ptr;
		return false;
	}

	struct FileMaskItem
	{
		ResourceManager::Strings stringId;
		const TCHAR* ext;
	};

	tstring getFileMaskString(const FileMaskItem* items);

	extern const FileMaskItem fileListsMask[];
	extern const FileMaskItem allFilesMask[];

	extern const GUID guidGetTTH;
	extern const GUID guidDcLstFromFolder;
};

class LastDir
{
	public:
		static const TStringList& get()
		{
			return dirs;
		}
		static void add(const tstring& dir)
		{
			auto i = find(dirs.begin(), dirs.end(), dir);
			if (i != dirs.end())
				dirs.erase(i);
			if (dirs.size() == 10)
				dirs.erase(dirs.begin());
			dirs.push_back(dir);
		}
		static void appendItems(OMenu& menu, int& count)
		{
			if (!dirs.empty())
			{
				menu.InsertSeparatorLast(TSTRING(PREVIOUS_FOLDERS));
				tstring tmp;
				for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
				{
					menu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TARGET + (++count), WinUtil::escapeMenu(*i, tmp).c_str());
				}
			}
		}

	private:
		static TStringList dirs;
};

#endif // WIN_UTIL_H

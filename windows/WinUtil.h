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

#include <functional>

#include <Richedit.h>
#include <atlctrls.h>

#include "resource.h"
#include "../client/Util.h"
#include "../client/SettingsManager.h"
#include "../client/MerkleTree.h"
#include "UserInfoSimple.h"
#include "OMenu.h"
#include "HIconWrapper.h"
#include "ImageLists.h"
#include "wtl_flylinkdc.h"

#define SHOW_POPUP(popup_key, msg, title) \
{ \
	if (POPUP_ENABLED(popup_key)) \
		MainFrame::ShowBalloonTip(msg, title); \
}

#define SHOW_POPUPF(popup_key, msg, title, flags) \
{ \
	if (POPUP_ENABLED(popup_key)) \
		MainFrame::ShowBalloonTip(msg, title, flags); \
}

#define SHOW_POPUP_EXT(popup_key, msg, popup_ext_key, ext_msg, ext_len, title) \
{ \
	if (POPUP_ENABLED(popup_key) && BOOLSETTING(popup_ext_key)) \
		MainFrame::ShowBalloonTip(msg + ext_msg.substr(0, ext_len), title); \
}

#define PLAY_SOUND(soundKey) WinUtil::playSound(SOUND_SETTING(soundKey))
#define PLAY_SOUND_BEEP(soundKey) { if (SOUND_BEEP_BOOLSETTING(soundKey)) WinUtil::playSound(SOUND_SETTING(SOUND_BEEPFILE), true); }

#ifdef USE_SET_LIST_COLOR_IN_SETTINGS
#define SET_LIST_COLOR_IN_SETTING(ctrlList) setListViewColors(ctrlList)
#else
#define SET_LIST_COLOR_IN_SETTING(ctrlList)
#endif

#define SET_MIN_MAX(x, y, z) \
	updown.Attach(GetDlgItem(x)); \
	updown.SetRange32(y, z); \
	updown.Detach();

// Some utilities for handling HLS colors, taken from Jean-Michel LE FOL's codeproject
// article on WTL OfficeXP Menus
typedef DWORD HLSCOLOR;
#define HLS(h,l,s) ((HLSCOLOR)(((BYTE)(h)|((WORD)((BYTE)(l))<<8))|(((DWORD)(BYTE)(s))<<16)))
#define HLS_H(hls) ((BYTE)(hls))
#define HLS_L(hls) ((BYTE)(((WORD)(hls)) >> 8))
#define HLS_S(hls) ((BYTE)((hls)>>16))

HLSCOLOR RGB2HLS(COLORREF rgb);
COLORREF HLS2RGB(HLSCOLOR hls);

COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S);
HLSCOLOR HLS_TRANSFORM2(HLSCOLOR hls, int percent_L, int percent_S);

extern const TCHAR* g_file_list_type;

struct Tags// [+] IRainman struct for links and BB codes
{
	explicit Tags(const TCHAR* _tag) : tag(_tag) { }
	const tstring tag;
};

#define EXT_URL_LIST() \
	Tags(_T("http://")), \
	Tags(_T("https://")), \
	Tags(_T("ftp://")), \
	Tags(_T("irc://")), \
	Tags(_T("skype:")), \
	Tags(_T("ed2k://")), \
	Tags(_T("mms://")), \
	Tags(_T("xmpp://")), \
	Tags(_T("nfs://")), \
	Tags(_T("mailto:")), \
	Tags(_T("www."))
/*[!] IRainman: "www." - this record is possible because function WinUtil::translateLinkToextProgramm
    automatically generates the type of protocol as http before transfer to browser*/

template <class T> inline void safe_unsubclass_window(T* p)
{
	dcassert(p->IsWindow());
	if (p != nullptr && p->IsWindow())
	{
		p->UnsubclassWindow();
	}
}

class FlatTabCtrl;
class UserCommand;

class Preview
{
	public:
		static const int MAX_PREVIEW_APPS = 100;
		static void init()
		{
			g_previewMenu.CreatePopupMenu();
		}
		static bool isPreviewMenu(const HMENU& handle)
		{
			return g_previewMenu.m_hMenu == handle;
		}
	
	protected:
		static void startMediaPreview(WORD wID, const QueueItemPtr& qi);
		
		static void startMediaPreview(WORD wID, const TTHValue& tth);
		                             
		static void startMediaPreview(WORD wID, const string& target);
		                             
		static void clearPreviewMenu();
		
		static void setupPreviewMenu(const string& target);
		static void runPreviewCommand(WORD wID, const string& target);
		
		static int g_previewAppsSize;
		static OMenu g_previewMenu;
		
		dcdrun(static bool _debugIsClean; static bool _debugIsActivated;)
};

class PreviewBaseHandler : public Preview
{
		/*
		1) Create a method onPreviewCommand in your class, which will call startMediaPreview for a necessary data.
		2) clearPreviewMenu()
		3) appendPreviewItems(yourMenu)
		4) setupPreviewMenu(yourMenu)
		5) activatePreviewItems(yourMenu)
		6) Before you destroy the menu in your class you will definitely need to call WinUtil::unlinkStaticMenus(yourMenu)
		*/
	protected:
		static const int MAX_PREVIEW_APPS = 100;

		BEGIN_MSG_MAP(PreviewBaseHandler)
		COMMAND_RANGE_HANDLER(IDC_PREVIEW_APP, IDC_PREVIEW_APP + MAX_PREVIEW_APPS - 1, onPreviewCommand)
		END_MSG_MAP()
		
		virtual LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;
		
		static void appendPreviewItems(OMenu& menu)
		{
			dcassert(_debugIsClean);
			dcdrun(_debugIsClean = false;)
			
			menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)g_previewMenu, CTSTRING(PREVIEW_MENU), g_iconBitmaps.getBitmap(IconBitmaps::PREVIEW, 0));
		}
		
		static void activatePreviewItems(OMenu& menu);
};

class InternetSearchBaseHandler
{
		/*
		1) Create a method onSearchFileInInternet in its class, which will call searchFileInInternet for a necessary data.
		2) appendInternetSearchItems(yourMenu)
		*/
	protected:
		BEGIN_MSG_MAP(InternetSearchBaseHandler)
		COMMAND_ID_HANDLER(IDC_SEARCH_FILE_IN_GOOGLE, onSearchFileOnInternet)
		COMMAND_ID_HANDLER(IDC_SEARCH_FILE_IN_YANDEX, onSearchFileOnInternet)
		END_MSG_MAP()

		virtual LRESULT onSearchFileOnInternet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;

		void appendInternetSearchItems(OMenu& menu);
		static void searchFileOnInternet(const WORD wID, const tstring& file);

		static void searchFileOnInternet(const WORD wID, const string& file)
		{
			searchFileOnInternet(wID, Text::toT(file));
		}
};

template < class T, int title, int ID = -1 >
class StaticFrame
{
	public:
		StaticFrame()
		{
		}
		virtual ~StaticFrame()
		{
			g_frame = nullptr;
		}
		
		static T* g_frame;

		static void toggleWindow()
		{
			if (g_frame == nullptr)
			{
				g_frame = new T();
				g_frame->CreateEx(WinUtil::g_mdiClient, g_frame->rcDefault, CTSTRING_I(ResourceManager::Strings(title)));
				WinUtil::setButtonPressed(ID, true);
			}
			else
			{
				// match the behavior of MainFrame::onSelected()
				HWND hWnd = g_frame->m_hWnd;
				if (isMDIChildActive(hWnd))
				{
					::PostMessage(hWnd, WM_CLOSE, NULL, NULL);
				}
				else if (g_frame->MDIGetActive() != hWnd)
				{
					MainFrame::getMainFrame()->MDIActivate(hWnd);
					WinUtil::setButtonPressed(ID, true);
				}
				else if (BOOLSETTING(TOGGLE_ACTIVE_WINDOW))
				{
					::SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
					g_frame->MDINext(hWnd);
					hWnd = g_frame->MDIGetActive();
					WinUtil::setButtonPressed(ID, true);
				}
				if (::IsIconic(hWnd))
					::ShowWindow(hWnd, SW_RESTORE);
			}
		}

		static void openWindow()
		{
			if (g_frame == nullptr)
			{
				g_frame = new T();
				g_frame->CreateEx(WinUtil::g_mdiClient, g_frame->rcDefault, CTSTRING_I(ResourceManager::Strings(title)));
				WinUtil::setButtonPressed(ID, true);
			}
			else
			{
				HWND hWnd = g_frame->m_hWnd;
				if (isMDIChildActive(hWnd))
					return;
				MainFrame::getMainFrame()->MDIActivate(hWnd);
				WinUtil::setButtonPressed(ID, true);
				if (::IsIconic(hWnd))
					::ShowWindow(hWnd, SW_RESTORE);
			}
		}

		static bool isMDIChildActive(HWND hWnd)
		{
			HWND wnd = MainFrame::getMainFrame()->MDIGetActive();
			dcassert(wnd != NULL);
			return (hWnd == wnd);
		}

		static void closeWindow()
		{
			if (g_frame)
			{
				::PostMessage(g_frame->m_hWnd, WM_CLOSE, NULL, NULL);
			}
		}
};

template<class T, int title, int ID>
T* StaticFrame<T, title, ID>::g_frame = NULL;

struct Colors
{
	static void init();
	static void uninit()
	{
		::DeleteObject(g_bgBrush);
	}

	static bool getColorFromString(const tstring& colorText, COLORREF& color);
	
	static CHARFORMAT2 g_TextStyleTimestamp;
	static CHARFORMAT2 g_ChatTextGeneral;
	static CHARFORMAT2 g_ChatTextOldHistory;
	static CHARFORMAT2 g_TextStyleMyNick;
	static CHARFORMAT2 g_ChatTextMyOwn;
	static CHARFORMAT2 g_ChatTextServer;
	static CHARFORMAT2 g_ChatTextSystem;
	static CHARFORMAT2 g_TextStyleBold;
	static CHARFORMAT2 g_TextStyleFavUsers;
	static CHARFORMAT2 g_TextStyleFavUsersBan;
	static CHARFORMAT2 g_TextStyleOPs;
	static CHARFORMAT2 g_TextStyleURL;
	static CHARFORMAT2 g_ChatTextPrivate;
	static CHARFORMAT2 g_ChatTextLog;
	
	static COLORREF g_textColor;
	static COLORREF g_bgColor;
	
	static HBRUSH g_bgBrush;
	static LRESULT setColor(const HDC hdc)
	{
		::SetBkColor(hdc, g_bgColor);
		::SetTextColor(hdc, g_textColor);
		return (LRESULT)g_bgBrush;
	}
};

static inline void setListViewColors(CListViewCtrl& ctrlList)
{
	ctrlList.SetBkColor(Colors::g_bgColor);
	ctrlList.SetTextBkColor(Colors::g_bgColor);
	ctrlList.SetTextColor(Colors::g_textColor);
}

struct Fonts
{
	static void init();
	static void uninit()
	{
		::DeleteObject(g_font);
		g_font = nullptr;
		::DeleteObject(g_boldFont);
		g_boldFont = nullptr;
		::DeleteObject(g_systemFont);
		g_systemFont = nullptr;
	}
	
	static void decodeFont(const tstring& setting, LOGFONT &dest);
	
	static int g_fontHeight;
	static int g_fontHeightPixl;
	static HFONT g_font;
	static HFONT g_boldFont;
	static HFONT g_systemFont;
};

class WinUtil
{
	public:
		struct TextItem
		{
			WORD itemID;
			ResourceManager::Strings translatedString;
		};
		
		static CMenu g_mainMenu;
		static OMenu g_copyHubMenu;
		
		static HIconWrapper g_banIconOnline;
		static HIconWrapper g_banIconOffline;
		
		static HWND g_mainWnd;
		static HWND g_mdiClient;
		static FlatTabCtrl* g_tabCtrl;
		static HHOOK g_hook;
		static bool g_isAppActive;
		
		static void init(HWND hWnd);
		static void uninit();
		
		static void escapeMenu(tstring& text);
		static const tstring& escapeMenu(const tstring& text, tstring& tmp);

		static void appendSeparator(HMENU menu);
		static void appendSeparator(OMenu& menu);

		static LONG getTextWidth(const tstring& str, HWND hWnd)
		{
			LONG sz = 0;
			if (str.length())
			{
				HFONT fnt = (HFONT) SendMessage(hWnd, WM_GETFONT, 0, 0);
				HDC dc = GetDC(hWnd);
				if (dc)
				{
					HGDIOBJ old = fnt ? SelectObject(dc, fnt) : nullptr;
					SIZE size;
					GetTextExtentPoint32(dc, str.c_str(), str.length(), &size);
					sz = size.cx;
					if (old) SelectObject(dc, old);
					ReleaseDC(hWnd, dc);
				}
			}
			return sz;
		}

		static LONG getTextWidth(const tstring& str, HDC dc)
		{
			SIZE sz = { 0, 0 };
			if (str.length())
				GetTextExtentPoint32(dc, str.c_str(), str.length(), &sz);
			return sz.cx;
		}
		
		static LONG getTextHeight(HWND wnd, HFONT fnt)
		{
			const HDC dc = ::GetDC(wnd);
			const LONG h = getTextHeight(dc, fnt);
			const int l_res = ::ReleaseDC(wnd, dc);
			dcassert(l_res);
			return h;
		}
		
		static LONG getTextHeight(HDC dc, HFONT fnt)
		{
			const HGDIOBJ old = ::SelectObject(dc, fnt);
			const LONG h = getTextHeight(dc);
			::SelectObject(dc, old);
			return h;
		}
		
		static LONG getTextHeight(HDC dc)
		{
			TEXTMETRIC tm = {0};
			const BOOL l_res = ::GetTextMetrics(dc, &tm);
			dcassert(l_res);
			return tm.tmHeight;
		}
		
		static void setClipboard(const tstring& str);
		static void setClipboard(const string& str)
		{
			setClipboard(Text::toT(str));
		}
		
		static uint32_t percent(int32_t x, uint8_t percent)
		{
			return x * percent / 100;
		}
		static void unlinkStaticMenus(OMenu &menu);
		
	private:
		dcdrun(static bool g_staticMenuUnlinked;)
	public:
	
		static tstring encodeFont(const LOGFONT& font);
		
		static bool browseFile(tstring& target, HWND owner = nullptr, bool save = true, const tstring& initialDir = Util::emptyStringT, const TCHAR* types = nullptr, const TCHAR* defExt = nullptr, const GUID* id = nullptr);
		static bool browseDirectory(tstring& target, HWND owner = nullptr, const GUID* id = nullptr);
		
		// Hash related
		static void copyMagnet(const TTHValue& /*aHash*/, const string& /*aFile*/, int64_t);
		
		static void searchHash(const TTHValue& hash);
		static void searchFile(const string& file);
		
		enum DefinedMagnetAction { MA_DEFAULT, MA_ASK, MA_DOWNLOAD, MA_SEARCH, MA_OPEN };
		
		// URL related
		static void registerHubUrlHandlers();
		static void registerMagnetHandler();
		static void registerDclstHandler();
		static void unregisterHubUrlHandlers();
		static void unregisterMagnetHandler();
		static void unregisterDclstHandler();
		static bool parseDchubUrl(const tstring& aUrl);
		static bool parseMagnetUri(const tstring& aUrl, DefinedMagnetAction Action = MA_DEFAULT);
		static void openFileList(const tstring& filename, DefinedMagnetAction Action = MA_DEFAULT); // [+] IRainman dclst support
		static bool hubUrlHandlersRegistered;
		static bool magnetHandlerRegistered;
		static bool dclstHandlerRegistered;
		static int textUnderCursor(POINT p, CEdit& ctrl, tstring& x);
		static void playSound(const string& soundFile, bool beep = false);
		static bool openLink(const tstring& url);
		static void openFile(const tstring& file);
		static void openFile(const TCHAR* file);
		static void openLog(const string& dir, const StringMap& params, const tstring& noLogMessage);
		
		static void openFolder(const tstring& file);
		
		//returns the position where the context menu should be
		//opened if it was invoked from the keyboard.
		//aPt is relative to the screen not the control.
		static void getContextMenuPos(CListViewCtrl& aList, POINT& aPt);
		static void getContextMenuPos(CTreeViewCtrl& aTree, POINT& aPt);
		static void getContextMenuPos(CEdit& aEdit,         POINT& aPt);
		
		static bool getUCParams(HWND parent, const UserCommand& cmd, StringMap& sm);
		
		/** @return Pair of hubnames as a string and a bool representing the user's online status */
		static pair<tstring, bool> getHubNames(const CID& cid, const string& hintUrl);
		static pair<tstring, bool> getHubNames(const UserPtr& u, const string& hintUrl);
		static pair<tstring, bool> getHubNames(const CID& cid, const string& hintUrl, bool priv);
		static pair<tstring, bool> getHubNames(const HintedUser& user);
		static int splitTokens(int* result, const string& tokens, int maxItems) noexcept;
		static int splitTokensWidth(int* result, const string& tokens, int maxItems, int defaultValue = 100) noexcept;
		static void saveHeaderOrder(CListViewCtrl& ctrl, SettingsManager::StrSetting order,
		                            SettingsManager::StrSetting widths, int n, int* indexes, int* sizes) noexcept; // !SMT!-UI todo: disable - this routine does not save column visibility
		static bool isShift()
		{
			return (GetKeyState(VK_SHIFT) & 0x8000) > 0;
		}
		static bool isAlt()
		{
			return (GetKeyState(VK_MENU) & 0x8000) > 0;
		}
		static bool isCtrl()
		{
			return (GetKeyState(VK_CONTROL) & 0x8000) > 0;
		}
		static bool isCtrlOrAlt()
		{
			return isCtrl() || isAlt();
		}
		
		template<class T> static HWND hiddenCreateEx(T& p) noexcept
		{
			const HWND active = (HWND)::SendMessage(g_mdiClient, WM_MDIGETACTIVE, 0, 0);
			CFlyLockWindowUpdate l(g_mdiClient);
			HWND ret = p.CreateEx(g_mdiClient);
			if (active && ::IsWindow(active))
			{
				::SendMessage(g_mdiClient, WM_MDIACTIVATE, (WPARAM)active, 0);
			}
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
		
		static bool shutDown(int action);
		static int setButtonPressed(int nID, bool bPressed = true);
		static void activateMDIChild(HWND hWnd);

		static string getWMPSpam(HWND playerWnd = NULL);
		static string getItunesSpam(HWND playerWnd = NULL);
		static string getMPCSpam();
		static string getWinampSpam(HWND playerWnd = NULL, int playerType = 0);
		static string getJASpam();
		
		static tstring getNicks(const CID& cid, const string& hintUrl);
		static tstring getNicks(const UserPtr& u, const string& hintUrl);
		static tstring getNicks(const CID& cid, const string& hintUrl, bool priv);
		static tstring getNicks(const HintedUser& user);
		
		static bool setExplorerTheme(HWND hWnd);
		static unsigned getListViewExStyle(bool checkboxes);
		static unsigned getTreeViewStyle();
#ifdef IRAINMAN_ENABLE_WHOIS
		static bool processWhoisMenu(WORD wID, const tstring& ip);
		static void appendWhoisMenu(OMenu& menu, const tstring& ip, bool useSubmenu);
#endif
		static void fillAdapterList(bool v6, CComboBox& bindCombo, const string& bindAddress);
		static string getSelectedAdapter(const CComboBox& bindCombo);
		static bool isTeredo();
		
		static void fillCharsetList(CComboBox& comboBox, int selected, bool onlyUTF8, bool inFavs);
		static int getSelectedCharset(const CComboBox& comboBox);

		static void GetTimeValues(CComboBox& p_ComboBox); // [+] InfinitySky.
	
		struct userStreamIterator
		{
			userStreamIterator() : position(0), length(0) {}
			
			unique_ptr<uint8_t[]> data;
			int position;
			int length;
		};
		
#ifdef SSA_SHELL_INTEGRATION
		static tstring getShellExtDllPath();
		static bool registerShellExt(bool unregister);
#endif
		static bool runElevated(HWND hwnd, LPCTSTR pszPath, LPCTSTR pszParameters = NULL, LPCTSTR pszDirectory = NULL);
		
		static bool autoRunShortcut(bool create);
		static bool isAutoRunShortcutExists();
		static tstring getAutoRunShortcutName();

		static void getWindowText(HWND hwnd, tstring& text);
		
		static void appendPrioItems(OMenu& menu, int idFirst);

	private:
		static int CALLBACK browseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lp*/, LPARAM pData);
		static bool createShortcut(const tstring& targetFile, const tstring& targetArgs, const tstring& linkFile, const tstring& description, int showMode, const tstring& workDir, const tstring& iconFile, int iconIndex);
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
			if (find(dirs.begin(), dirs.end(), dir) != dirs.end())
			{
				return;
			}
			if (dirs.size() == 10)
			{
				dirs.erase(dirs.begin());
			}
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

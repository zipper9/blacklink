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
#include "../client/DCPlusPlus.h"
#include "../client/Util.h"
#include "../client/SettingsManager.h"
#include "../client/MerkleTree.h"
#include "../client/HintedUser.h"
#include "../client/CompatibilityManager.h"
#include "UserInfoSimple.h"
#include "OMenu.h"
#include "HIconWrapper.h"
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

static inline void setListViewExtStyle(CListViewCtrl& ctrlList, bool gridLines, bool checkBoxes)
{
	ctrlList. SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP |
	                                   (gridLines ? LVS_EX_GRIDLINES /*TODO LVS_OWNERDRAWFIXED*/ : 0) |
	                                   (checkBoxes ? LVS_EX_CHECKBOXES : 0));
}

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

class Preview // [+] IRainman fix.
{
	public:
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
		
		static UINT getPreviewMenuIndex();
		static void setupPreviewMenu(const string& target);
		static void runPreviewCommand(WORD wID, const string& target);
		
		static int g_previewAppsSize;
		static OMenu g_previewMenu;
		
		dcdrun(static bool _debugIsClean; static bool _debugIsActivated;)
};

template<class T, bool OPEN_EXISTING_FILE = false>
class PreviewBaseHandler : public Preview // [+] IRainman fix.
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
		COMMAND_ID_HANDLER(IDC_PREVIEW_APP_INT, onPreviewCommand)
		COMMAND_ID_HANDLER(IDC_STARTVIEW_EXISTING_FILE, onPreviewCommand)
		END_MSG_MAP()
		
		virtual LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;
		
		static void appendPreviewItems(OMenu& p_menu)
		{
			dcassert(_debugIsClean);
			dcdrun(_debugIsClean = false;)
			
			p_menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)g_previewMenu, CTSTRING(PREVIEW_MENU));
			if (OPEN_EXISTING_FILE)
			{
				p_menu.AppendMenu(MF_STRING, IDC_STARTVIEW_EXISTING_FILE, CTSTRING(STARTVIEW_EXISTING_FILE));
			}
		}
		
		static void activatePreviewItems(OMenu& p_menu, const bool existingFile = false)
		{
			dcassert(!_debugIsActivated);
			dcdrun(_debugIsActivated = true;)
			
			p_menu.EnableMenuItem(getPreviewMenuIndex(), g_previewMenu.GetMenuItemCount() > 0 ? MFS_ENABLED : MFS_DISABLED);
			if (OPEN_EXISTING_FILE)
			{
				p_menu.EnableMenuItem(IDC_STARTVIEW_EXISTING_FILE, existingFile ? MFS_ENABLED : MFS_DISABLED);
			}
		}
};

template<class T>
class InternetSearchBaseHandler // [+] IRainman fix.
{
		/*
		1) Create a method onSearchFileInInternet in its class, which will call searchFileInInternet for a necessary data.
		2) appendInternetSearchItems(yourMenu)
		*/
	protected:
		BEGIN_MSG_MAP(InternetSearchBaseHandler)
		COMMAND_ID_HANDLER(IDC_SEARCH_FILE_IN_GOOGLE, onSearchFileInInternet)
		COMMAND_ID_HANDLER(IDC_SEARCH_FILE_IN_YANDEX, onSearchFileInInternet)
		END_MSG_MAP()
		
		virtual LRESULT onSearchFileInInternet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;
		
		void appendInternetSearchItems(CMenu& p_menu)
		{
			p_menu.AppendMenu(MF_STRING, IDC_SEARCH_FILE_IN_GOOGLE, CTSTRING(SEARCH_FILE_IN_GOOGLE));
			p_menu.AppendMenu(MF_STRING, IDC_SEARCH_FILE_IN_YANDEX, CTSTRING(SEARCH_FILE_IN_YANDEX));
		}
		
		static void searchFileInInternet(const WORD wID, const tstring& file)
		{
			tstring url;
			switch (wID)
			{
				case IDC_SEARCH_FILE_IN_GOOGLE:
					url += _T("https://www.google.com/search?hl=") + WinUtil::GetLang() + _T("&q=");
					break;
				case IDC_SEARCH_FILE_IN_YANDEX:
					url += _T("http://yandex.ru/yandsearch?text=");
					break;
				default:
					return;
			}
			url += file;
			WinUtil::openFile(url);
		}
		static void searchFileInInternet(const WORD wID, const string& file)
		{
			searchFileInInternet(wID, Text::toT(file));
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
		static void openWindow()
		{
			if (g_frame == nullptr)
			{
				g_frame = new T();
				// g_frame->m_title_id = title;
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
		static bool isMDIChildActive(HWND hWnd)
		{
			HWND wnd = MainFrame::getMainFrame()->MDIGetActive();
			dcassert(wnd != NULL);
			return (hWnd == wnd);
		}
		// !SMT!-S
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

	enum Mask
	{
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		IS_AUTOBAN          = 0x0003,
		IS_AUTOBAN_ON       = 0x0001,
#endif
		IS_FAVORITE         = 0x0003 << 2,
		IS_FAVORITE_ON      = 0x0001 << 2,
		IS_BAN              = 0x0003 << 4,
		IS_BAN_ON           = 0x0001 << 4,
		IS_RESERVED_SLOT    = 0x0003 << 6,
		IS_RESERVED_SLOT_ON = 0x0001 << 6,
		IS_IGNORED_USER     = 0x0003 << 8,
		IS_IGNORED_USER_ON  = 0x0001 << 8
	};

	static void getUserColor(bool isOp, const UserPtr& user, COLORREF& fg, COLORREF& bg, unsigned short& flags, const OnlineUserPtr& onlineUser);
	
	inline static COLORREF getAlternativBkColor(LPNMLVCUSTOMDRAW cd)
	{
		return Colors::g_bgColor;
	}
	
	// [+] SSA
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
	static LRESULT setColor(const HDC p_hdc)
	{
		::SetBkColor(p_hdc, g_bgColor);
		::SetTextColor(p_hdc, g_textColor);
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
		::DeleteObject(g_halfFont);
		g_halfFont = nullptr;
		::DeleteObject(g_systemFont);
		g_systemFont = nullptr;
	}
	
	static void decodeFont(const tstring& setting, LOGFONT &dest);
	
	static int g_fontHeight;
	static int g_fontHeightPixl;
	static HFONT g_font;
	static HFONT g_boldFont;
	static HFONT g_systemFont;
	static HFONT g_halfFont;
};

class LastDir
{
	public:
		static const TStringList& get()
		{
			return g_dirs;
		}
		static void add(const tstring& dir)
		{
			if (find(g_dirs.begin(), g_dirs.end(), dir) != g_dirs.end())
			{
				return;
			}
			if (g_dirs.size() == 10)
			{
				g_dirs.erase(g_dirs.begin());
			}
			g_dirs.push_back(dir);
		}
		static void appendItem(OMenu& p_menu, int& p_num)
		{
			if (!g_dirs.empty())
			{
				p_menu.InsertSeparatorLast(TSTRING(PREVIOUS_FOLDERS));
				for (auto i = g_dirs.cbegin(); i != g_dirs.cend(); ++i)
				{
					p_menu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TARGET + (++p_num), Text::toLabel(*i).c_str());
				}
			}
		}
	private:
		static TStringList g_dirs;
};

class WinUtil
{
	public:
		// !SMT!-UI search user by exact share size
		//typedef std::unordered_multimap<uint64_t, UserPtr> ShareMap;
//		typedef ShareMap::iterator ShareIter;
		//static ShareMap UsersShare;
		
		struct TextItem
		{
			WORD itemID;
			ResourceManager::Strings translatedString;
		};
		
		static CMenu g_mainMenu;
		static OMenu g_copyHubMenu; // [+] IRainman fix.
		
		static HIconWrapper g_banIconOnline; // !SMT!-UI
		static HIconWrapper g_banIconOffline; // !SMT!-UI
		static HIconWrapper g_hMedicalIcon;
		//static HIconWrapper g_hCrutchIcon;
		static HIconWrapper g_hFirewallIcon;
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
		static HIconWrapper g_hClockIcon;
#endif
		
		static std::unique_ptr<HIconWrapper> g_HubOnIcon;
		static std::unique_ptr<HIconWrapper> g_HubOffIcon;
		static std::unique_ptr<HIconWrapper> g_HubDDoSIcon;
		static HIconWrapper g_hThermometerIcon;
		static void initThemeIcons();
		
		static HWND g_mainWnd;
		static HWND g_mdiClient;
		static FlatTabCtrl* g_tabCtrl;
		static HHOOK g_hook;
		static bool g_isAppActive;
		
		static void init(HWND hWnd);
		static void uninit();
		
		static int GetMenuItemPosition(const CMenu &p_menu, UINT_PTR p_IDItem = 0); // [+] SCALOlaz
		
		static LONG getTextWidth(const tstring& str, HWND hWnd)
		{
			LONG sz = 0;
			//dcassert(str.length());
			if (str.length())
			{
				const HDC dc = ::GetDC(hWnd);
				sz = getTextWidth(str, dc);
				const int l_res = ::ReleaseDC(g_mainWnd, dc);
				dcassert(l_res);
			}
			return sz;
		}
		static LONG getTextWidth(const tstring& str, HDC dc)
		{
			SIZE sz = { 0, 0 };
			//dcassert(str.length());
			if (str.length())
			{
				::GetTextExtentPoint32(dc, str.c_str(), str.length(), &sz); //-V107
			}
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
		static void unlinkStaticMenus(CMenu &menu); // !SMT!-UI
		
	private:
		dcdrun(static bool g_staticMenuUnlinked;)
	public:
	
		static tstring encodeFont(const LOGFONT& font);
		
		static bool browseFile(tstring& target, HWND owner = NULL, bool save = true, const tstring& initialDir = Util::emptyStringT, const TCHAR* types = NULL, const TCHAR* defExt = NULL);
		static bool browseDirectory(tstring& target, HWND owner = NULL);
		
		// Hash related
		static void copyMagnet(const TTHValue& /*aHash*/, const string& /*aFile*/, int64_t);
		
		static void searchHash(const TTHValue& hash);
		static void searchFile(const string& file);
		
		enum DefinedMagnetAction { MA_DEFAULT, MA_ASK, MA_DOWNLOAD, MA_SEARCH, MA_OPEN };
		
		// URL related
		static void registerDchubHandler();
		static void registerNMDCSHandler();
		static void registerADChubHandler();
		static void registerADCShubHandler();
		static void registerMagnetHandler();
		static void registerDclstHandler();// [+] IRainman dclst support
		static void unRegisterDchubHandler();
		static void unRegisterNMDCSHandler();
		static void unRegisterADChubHandler();
		static void unRegisterADCShubHandler();
		static void unRegisterMagnetHandler();
		static void unRegisterDclstHandler();// [+] IRainman dclst support
		static bool parseDchubUrl(const tstring& aUrl);// [!] IRainman stop copy-past!
		//static void parseADChubUrl(const tstring& /*aUrl*/, bool secure);[-] IRainman stop copy-past!
		static bool parseMagnetUri(const tstring& aUrl, DefinedMagnetAction Action = MA_DEFAULT);
		static void OpenFileList(const tstring& filename, DefinedMagnetAction Action = MA_DEFAULT); // [+] IRainman dclst support
		static bool parseDBLClick(const tstring& /*aString*/, string::size_type start, string::size_type end);
		static bool urlDcADCRegistered;
		static bool urlMagnetRegistered;
		static bool DclstRegistered;
		static int textUnderCursor(POINT p, CEdit& ctrl, tstring& x);
		static void translateLinkToextProgramm(const tstring& url, const tstring& p_Extension = Util::emptyStringT, const tstring& p_openCmd = Util::emptyStringT);//[+]FlylinkDC
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
		
		static tstring escapeMenu(tstring str)
		{
			string::size_type i = 0;
			while ((i = str.find(_T('&'), i)) != string::npos)
			{
				str.insert(str.begin() + i, 1, _T('&'));
				i += 2;
			}
			return str;
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
			{
				::SetDlgItemText(page, textItems[i].itemID,
				                 Text::toT(ResourceManager::getString(textItems[i].translatedString)).c_str());
			}
		}
		
		static bool shutDown(int action);
		static int setButtonPressed(int nID, bool bPressed = true);
//TODO      static bool checkIsButtonPressed(int nID);// [+] IRainman

		static string getWMPSpam(HWND playerWnd = NULL);
		static string getItunesSpam(HWND playerWnd = NULL);
		static string getMPCSpam();
		static string getWinampSpam(HWND playerWnd = NULL, int playerType = 0);
		static string getJASpam();
		
		
// FDM extension
		static tstring getNicks(const CID& cid, const string& hintUrl);
		static tstring getNicks(const UserPtr& u, const string& hintUrl);
		static tstring getNicks(const CID& cid, const string& hintUrl, bool priv);
		static tstring getNicks(const HintedUser& user);
		
		static bool isUseExplorerTheme();
		static void SetWindowThemeExplorer(HWND p_hWnd);
#ifdef IRAINMAN_ENABLE_WHOIS
		static void CheckOnWhoisIP(WORD wID, const tstring& whoisIP);
		static void AppendMenuOnWhoisIP(CMenu &p_menuname, const tstring& p_IP, const bool p_inSubmenu);
#endif
		static void fillAdapterList(bool v6, CComboBox& bindCombo, const string& bindAddress);
		static string getSelectedAdapter(const CComboBox& bindCombo);
		static bool isTeredo();
		
		static void fillCharsetList(CComboBox& comboBox, int selected, bool onlyUTF8);
		static int getSelectedCharset(const CComboBox& comboBox);

		static void GetTimeValues(CComboBox& p_ComboBox); // [+] InfinitySky.
	
		static tstring getFilenameFromString(const tstring& filename);
		
		struct userStreamIterator
		{
			userStreamIterator() : position(0), length(0) {}
			
			unique_ptr<uint8_t[]> data;
			int position;
			int length;
		};
		
#ifdef SSA_SHELL_INTEGRATION
		static wstring getShellExtDllPath();
		static bool makeShellIntegration(bool isNeedUnregistred);
#endif
		static bool runElevated(HWND hwnd, LPCTSTR pszPath, LPCTSTR pszParameters = NULL, LPCTSTR pszDirectory = NULL);
		
		template<class M>
		static string getDataFromMap(int p_ComboIndex, const M& p_Map)
		{
			if (p_ComboIndex >= 0)
			{
				int j = 0;
				for (auto i = p_Map.cbegin(); i != p_Map.cend(); ++i, ++j)
					if (p_ComboIndex == j)
						return i->second;
			}
			return Util::emptyString;
		}
		
		template<class M, typename T>
		static int getIndexFromMap(const M& p_Map, const T& p_Data)
		{
			int j = 0;
			for (auto i = p_Map.cbegin(); i != p_Map.cend(); ++i, ++j)
				if (p_Data == i->second)
					return j;
			return -1;
		}
		
		static void safe_sh_free(void* p_ptr)
		{
			IMalloc * l_imalloc = nullptr;
			if (SUCCEEDED(SHGetMalloc(&l_imalloc)))
			{
				l_imalloc->Free(p_ptr);
				safe_release(l_imalloc);
			}
		}
		
		static bool AutoRunShortCut(bool bCreate);
		static bool IsAutoRunShortCutExists();
		static tstring GetAutoRunShortCutName();
		static tstring GetLang()
		{
			return Text::toT(Util::getLang());
		}

		static void getWindowText(HWND hwnd, tstring& text);
		
		// FIXME: we should not use ATL strings
		static tstring fromAtlString(const CAtlString& str)
		{
			return tstring(str, str.GetLength());
		}

		// FIXME: we should not use ATL strings
		static CAtlString toAtlString(const tstring& str)
		{
			dcassert(!str.empty());
			if (!str.empty())
			{
				return CAtlString(str.c_str(), str.size());
			}
			else
			{
				return CAtlString();
			}
		}

		// FIXME: we should not use ATL strings
		static CAtlString toAtlString(const string& str)
		{
			return toAtlString(Text::toT(str));
		}

		static void appendPrioItems(OMenu& menu, int idFirst);

	private:
		static int CALLBACK browseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lp*/, LPARAM pData);
		static inline TCHAR CharTranscode(const TCHAR msg); // [+] Drakon. Transcoding text between Russian & English
		static bool   CreateShortCut(const tstring& pszTargetfile, const tstring& pszTargetargs, const tstring& pszLinkfile, const tstring& pszDescription, int iShowmode, const tstring& pszCurdir, const tstring& pszIconfile, int iIconindex);
		
		static DWORD CALLBACK EditStreamCallback(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb); // [+] SSA
};

#endif // WIN_UTIL_H

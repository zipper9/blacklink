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

#ifndef PUBLIC_HUBS_FRM_H
#define PUBLIC_HUBS_FRM_H

#include "FlatTabCtrl.h"
#include "StaticFrame.h"
#include "TypedListViewCtrl.h"
#include "../client/HublistManager.h"

#define HUB_FILTER_MESSAGE_MAP 8
#define HUB_LIST_MESSAGE_MAP 10

class PublicHubsFrame : public MDITabChildWindowImpl<PublicHubsFrame>,
	public StaticFrame<PublicHubsFrame, ResourceManager::PUBLIC_HUBS, IDC_PUBLIC_HUBS>,
	private HublistManagerListener,
	private ClientManagerListener,
	private FavoriteManagerListener,
	private SettingsManagerListener,
	public CMessageFilter
{
	public:
		PublicHubsFrame();
		PublicHubsFrame(const PublicHubsFrame&) = delete;
		PublicHubsFrame& operator= (const PublicHubsFrame&) = delete;

		enum
		{
			COLUMN_FIRST,
			COLUMN_NAME = COLUMN_FIRST,
			COLUMN_DESCRIPTION,
			COLUMN_USERS,
			COLUMN_SERVER,
			COLUMN_COUNTRY,
			COLUMN_SHARED,
			COLUMN_MINSHARE,
			COLUMN_MINSLOTS,
			COLUMN_MAXHUBS,
			COLUMN_MAXUSERS,
			COLUMN_RELIABILITY,
			COLUMN_RATING,
			COLUMN_ENCODING,
			COLUMN_SECURE,
			COLUMN_SECURE_URL,
			COLUMN_WEBSITE,
			COLUMN_EMAIL,
			COLUMN_SOFTWARE,
			COLUMN_NETWORK,
			COLUMN_LAST
		};

		static CFrameWndClassInfo& GetWndClassInfo();

		typedef MDITabChildWindowImpl<PublicHubsFrame> baseClass;
		BEGIN_MSG_MAP(PublicHubsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_FILTER_FOCUS, onFilterFocus)
		COMMAND_ID_HANDLER(IDC_ADD, onAdd)
		COMMAND_ID_HANDLER(IDC_REM_AS_FAVORITE, onRemoveFav)
		COMMAND_ID_HANDLER(IDC_REFRESH, onClickedRefresh)
		COMMAND_ID_HANDLER(IDC_PUB_LIST_CONFIG, onClickedConfigure)
		COMMAND_ID_HANDLER(IDC_CONNECT, onClickedConnect)
		COMMAND_RANGE_HANDLER(IDC_COPY, IDC_COPY + COLUMN_LAST - 1, onCopy)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_GETDISPINFO, ctrlHubs.onGetDispInfo)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_COLUMNCLICK, ctrlHubs.onColumnClick)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_GETINFOTIP, ctrlHubs.onInfoTip)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_RETURN, onEnter)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_DBLCLK, onDoubleClickHublist)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_CUSTOMDRAW, onCustomDraw)
		COMMAND_HANDLER(IDC_PUB_LIST_DROPDOWN, CBN_SELCHANGE, onListSelChanged)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(HUB_LIST_MESSAGE_MAP)
		ALT_MSG_MAP(HUB_FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
		END_MSG_MAP()
		
		LRESULT onFilterChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDoubleClickHublist(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onEnter(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onFilterFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemoveFav(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onClickedRefresh(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
		LRESULT onClickedConfigure(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
		LRESULT onClickedConnect(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onListSelChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		bool checkNick();
		
		LRESULT onCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		
		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlHubs.SetFocus();
			return 0;
		}

		virtual BOOL PreTranslateMessage(MSG* pMsg) override;

	private:
		enum
		{
			WPARAM_UPDATE_STATE = 1,
			WPARAM_PROCESS_REDIRECT,
			WPARAM_HUB_CONNECTED,
			WPARAM_HUB_DISCONNECTED,
			WPARAM_FAVORITE_ADDED,
			WPARAM_FAVORITE_REMOVED
		};
		
		enum FilterModes
		{
			NONE,
			EQUAL,
			GREATER_EQUAL,
			LESS_EQUAL,
			GREATER,
			LESS,
			NOT_EQUAL
		};

		class HubInfo
		{
			public:
				const tstring& getText(int col) const;
				static int compareItems(const HubInfo* a, const HubInfo* b, int col);
				int getImageIndex() const { return favorite ? 0 : -1; }
				static int getStateImageIndex() { return 0; }
				void update(const HubEntry& hub);
				void setOnline(bool flag) { online = flag; }
				bool isOnline() const { return online; }
				const string& getHubUrl() const { return hubUrl; }
				const string& getSecureHubUrl() const { return secureHubUrl; }
				bool isFavorite() const { return favorite; }
				void setFavorite(bool flag) { favorite = flag; }
				int getCountryIndex() const { return countryIndex; }

			private:
				string hubUrl;
				string secureHubUrl;
				tstring text[COLUMN_LAST];
				int countryIndex;
				int users;
				int maxUsers;
				int maxHubs;
				int minSlots;
				int rating;
				int64_t shared;
				int64_t minShare;
				double reliability;
				bool online;
				bool favorite;
		};
		
		int visibleHubs;
		int users;
		CStatusBarCtrl ctrlStatus;
		CButton ctrlConfigure;
		CButton ctrlRefresh;
		CButton ctrlLists;	
		CButton ctrlFilterDesc;
		CEdit ctrlFilter;
		OMenu hubsMenu;
		OMenu copyMenu;
		
		CContainedWindow filterContainer;
		CComboBox ctrlPubLists;
		CComboBox ctrlFilterSel;
		TypedListViewCtrl<HubInfo, IDC_HUB> ctrlHubs;
		CustomDrawHelpers::CustomDrawState customDrawState;

		int xdu, ydu;
		int buttonWidth, buttonHeight;
		int comboHeight;
		int editHeight;
		int boxHeight;
		int groupBoxOffset;
		int horizOffset, vertOffset;
		int margin;

		CContainedWindow listContainer;
		
		vector<HublistManager::HubListInfo> hubLists;
		uint64_t selectedHubList;
		string filter; // converted to lowercase
		
		static const int columnId[];

		HubInfo* findHub(const string& url, int* pos) const;
		string getPubServer(int pos) const
		{
			return getPubServer(ctrlHubs.getItemData(pos));
		}
		string getPubServer(const HubInfo* data) const
		{
			return data ? Util::formatDchubUrl(Text::fromT(data->getText(COLUMN_SERVER))) : Util::emptyString;
		}
		void openHub(int ind);

		void updateStatus();
		void updateList(const HubEntry::List &hubs);
		void updateDropDown();
		void showStatus(const HublistManager::HubListInfo &info);
		void onListSelChanged();
		void redraw();

		bool parseFilter(FilterModes& mode, double& size);
		bool matchFilter(const HubEntry& entry, int sel, bool doSizeCompare, const FilterModes& mode, const double& size);

		static bool isFavorite(const HubInfo* data);

		void on(SettingsManagerListener::Repaint) override;

		void on(HublistManagerListener::StateChanged, uint64_t id) noexcept override;
		void on(HublistManagerListener::Redirected, uint64_t id) noexcept override;

		// ClientManagerListener
		void on(ClientConnected, const Client* c) noexcept override;
		void on(ClientDisconnected, const Client* c) noexcept override;

		// FavoriteManagerListener
		void on(FavoriteAdded, const FavoriteHubEntry*) noexcept override;
		void on(FavoriteRemoved, const FavoriteHubEntry*) noexcept override;
};

#endif // !defined(PUBLIC_HUBS_FRM_H)

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
#include "ExListViewCtrl.h"
#include "../client/HublistManager.h"

#define HUB_FILTER_MESSAGE_MAP 8
#define HUB_LIST_MESSAGE_MAP 10

class PublicHubsFrame : public MDITabChildWindowImpl<PublicHubsFrame>,
	public StaticFrame<PublicHubsFrame, ResourceManager::PUBLIC_HUBS, ID_FILE_CONNECT>,
	public HublistManagerListener,
	private SettingsManagerListener
{
	public:
		PublicHubsFrame() : users(0), visibleHubs(0)
			, filterContainer(WC_EDIT, this, HUB_FILTER_MESSAGE_MAP)
			, listContainer(WC_LISTVIEW, this, HUB_LIST_MESSAGE_MAP)
		{
		}
		
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
			COLUMN_LAST
		};
		
		DECLARE_FRAME_WND_CLASS_EX(_T("PublicHubsFrame"), IDR_INTERNET_HUBS, 0, COLOR_3DFACE);
		
		typedef MDITabChildWindowImpl<PublicHubsFrame> baseClass;
		BEGIN_MSG_MAP(PublicHubsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
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
		COMMAND_ID_HANDLER(IDC_COPY_HUB, onCopyHub);
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_COLUMNCLICK, onColumnClickHublist)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_RETURN, onEnter)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_DBLCLK, onDoubleClickHublist)
		//NOTIFY_HANDLER(IDC_HUBLIST, NM_CUSTOMDRAW, ctrlHubs.onCustomDraw) // [+] IRainman
		NOTIFY_HANDLER(IDC_HUBLIST, NM_CUSTOMDRAW, onCustomDraw)
		COMMAND_HANDLER(IDC_PUB_LIST_DROPDOWN, CBN_SELCHANGE, onListSelChanged)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(HUB_LIST_MESSAGE_MAP)
		ALT_MSG_MAP(HUB_FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
		END_MSG_MAP()
		
		LRESULT onFilterChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
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
		LRESULT onCopyHub(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onListSelChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
		LRESULT onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
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
		
	private:
		enum
		{
			WPARAM_UPDATE_STATE = 1,
			WPARAM_PROCESS_REDIRECT
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
		
		int visibleHubs;
		int users;
		CStatusBarCtrl ctrlStatus;
		CButton ctrlConfigure;
		CButton ctrlRefresh;
		CButton ctrlLists;	
		CButton ctrlFilterDesc;
		CEdit ctrlFilter;
		CMenu hubsMenu;
		
		CContainedWindow filterContainer;
		CComboBox ctrlPubLists;
		CComboBox ctrlFilterSel;
		ExListViewCtrl ctrlHubs;

		CContainedWindow listContainer;
		
		vector<HublistManager::HubListInfo> hubLists;
		uint64_t selectedHubList;
		string filter; // converted to lowercase
		
		StringSet onlineHubs;
		bool isOnline(const string& url) const
		{
			return onlineHubs.find(url) != onlineHubs.end();
		}
		
		static int columnIndexes[];
		static int columnSizes[];
		static HIconWrapper frameIcon;
		
		const string getPubServer(int pos) const
		{
			return ctrlHubs.ExGetItemText(pos, COLUMN_SERVER);
		}
		void openHub(int ind);
		
		void updateStatus();
		void updateList(const HubEntry::List &hubs);
		void updateDropDown();
		void showStatus(const HublistManager::HubListInfo &info);
		void onListSelChanged();

		bool parseFilter(FilterModes& mode, double& size);
		bool matchFilter(const HubEntry& entry, int sel, bool doSizeCompare, const FilterModes& mode, const double& size);
		
		void on(SettingsManagerListener::Repaint) override;

		void on(HublistManagerListener::StateChanged, uint64_t id) noexcept override;
		void on(HublistManagerListener::Redirected, uint64_t id) noexcept override;
};

#endif // !defined(PUBLIC_HUBS_FRM_H)

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

#ifndef FAVORITE_HUBS_FRM_H
#define FAVORITE_HUBS_FRM_H

#include "FlatTabCtrl.h"
#include "StaticFrame.h"
#include "ExListViewCtrl.h"
#include "TimerHelper.h"

#include "../client/FavoriteManagerListener.h"
#include "../client/Client.h"

#define SERVER_MESSAGE_MAP 7

class FavoriteHubsFrame :
	public MDITabChildWindowImpl<FavoriteHubsFrame>,
	public StaticFrame<FavoriteHubsFrame, ResourceManager::FAVORITE_HUBS, IDC_FAVORITES>,
	private FavoriteManagerListener,
	private ClientManagerListener,
	private TimerHelper,
	private SettingsManagerListener,
	public CMessageFilter
{
	public:
		typedef MDITabChildWindowImpl<FavoriteHubsFrame> baseClass;
		
		FavoriteHubsFrame();
		
		static CFrameWndClassInfo& GetWndClassInfo();
		
		BEGIN_MSG_MAP(FavoriteHubsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer);
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_CONNECT, onClickedConnect)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_NEWFAV, onNew)
		COMMAND_ID_HANDLER(IDC_MOVE_UP, onMoveUp);
		COMMAND_ID_HANDLER(IDC_MOVE_DOWN, onMoveDown);
		COMMAND_ID_HANDLER(IDC_OPEN_HUB_LOG, onOpenHubLog)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_MANAGE_GROUPS, onManageGroups)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_CUSTOMDRAW, ctrlHubs.onCustomDraw)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_DBLCLK, onDoubleClickHublist)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_COLUMNCLICK, onColumnClickHublist)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onNew(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onMoveUp(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onMoveDown(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onManageGroups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		bool checkNick();
		void UpdateLayout(BOOL resizeBars = TRUE);
		
		LRESULT onEnter(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
		{
			openSelected();
			return 0;
		}
		
		LRESULT onClickedConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			openSelected();
			return 0;
		}
		
		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlHubs.SetFocus();
			return 0;
		}
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}

		virtual BOOL PreTranslateMessage(MSG* pMsg) override;

	private:
		enum
		{
			COLUMN_FIRST,
			COLUMN_NAME = COLUMN_FIRST,
			COLUMN_DESCRIPTION,
			COLUMN_NICK,
			COLUMN_PASSWORD,
			COLUMN_SERVER,
			COLUMN_USERDESCRIPTION,
			COLUMN_EMAIL,
			COLUMN_SHARE_GROUP,
			COLUMN_CONNECTION_STATUS,
			COLUMN_LAST_CONNECTED,
			COLUMN_LAST
		};
		
		enum
		{
			HUB_CONNECTED,
			HUB_DISCONNECTED
		};

		struct StateKeeper
		{
				StateKeeper(ExListViewCtrl& hubs, bool ensureVisible = true);
				~StateKeeper();
				
				const std::vector<int>& getSelection() const;
				
			private:
				ExListViewCtrl& hubs;
				bool ensureVisible;
				std::vector<int> selected;
				int scroll;
		};
		
		CButton ctrlConnect;
		CButton ctrlRemove;
		CButton ctrlNew;
		CButton ctrlProps;
		CButton ctrlUp;
		CButton ctrlDown;
		CButton ctrlManageGroups;
		OMenu hubsMenu;
		CImageList onlineStatusImg;
		ExListViewCtrl ctrlHubs;

		int xdu, ydu;
		int buttonWidth, buttonHeight, buttonSpace, buttonDeltaWidth;
		int vertMargin, horizMargin;

		StringSet onlineHubs;
		bool noSave;

		static int columnSizes[COLUMN_LAST];
		static int columnIndexes[COLUMN_LAST];

		static HIconWrapper stateIconOn, stateIconOff;

		static tstring printConnectionStatus(const ConnectionStatus& cs, time_t curTime);
		static tstring printLastConnected(const ConnectionStatus& cs);
		void addEntryL(const FavoriteHubEntry* entry, int pos, int groupIndex, time_t now);
		void handleMove(bool up);
		TStringList getSortedGroups() const;
		void fillList();
		void fillList(const TStringList& groups);
		void openSelected();
		int findItem(int id) const;
		
		void on(FavoriteAdded, const FavoriteHubEntry* entry) noexcept override;
		void on(FavoriteRemoved, const FavoriteHubEntry* entry) noexcept override;
		void on(FavoriteChanged, const FavoriteHubEntry* entry) noexcept override;

		void on(SettingsManagerListener::Repaint) override;
		
		// ClientManagerListener
		void on(ClientConnected, const Client* c) noexcept override
		{
			if (!ClientManager::isBeforeShutdown())
				WinUtil::postSpeakerMsg(m_hWnd, HUB_CONNECTED, new string(c->getHubUrl()));
		}

		void on(ClientDisconnected, const Client* c) noexcept override
		{
			if (!ClientManager::isBeforeShutdown())
				WinUtil::postSpeakerMsg(m_hWnd, HUB_DISCONNECTED, new string(c->getHubUrl()));
		}

		bool isOnline(const string& hubUrl) const
		{
			return onlineHubs.find(hubUrl) != onlineHubs.end();
		}
};

#endif // !defined(FAVORITE_HUBS_FRM_H)

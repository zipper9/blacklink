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

#ifndef SPY_FRAME_H
#define SPY_FRAME_H

#include "../client/ClientManager.h"
#include "../client/TaskQueue.h"

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "CustomDrawHelpers.h"
#include "TimerHelper.h"

#define SPYFRAME_IGNORETTH_MESSAGE_MAP 7
#define SPYFRAME_SHOW_NICK 8
#define SPYFRAME_LOG_FILE 9

class SpyFrame : public MDITabChildWindowImpl<SpyFrame>,
	public StaticFrame<SpyFrame, ResourceManager::SEARCH_SPY, IDC_SEARCH_SPY>,
	private ClientManagerListener,
	private SettingsManagerListener
{
	public:
		SpyFrame();
		~SpyFrame();
		
		SpyFrame(const SpyFrame&) = delete;
		SpyFrame& operator= (const SpyFrame&) = delete;
		
		DECLARE_FRAME_WND_CLASS_EX(_T("SpyFrame"), IDR_SPY, 0, COLOR_3DFACE)
		
		typedef MDITabChildWindowImpl<SpyFrame> baseClass;
		BEGIN_MSG_MAP(SpyFrame)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_GETDISPINFO, ctrlSearches.onGetDispInfo)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_COLUMNCLICK, ctrlSearches.onColumnClick)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_GETINFOTIP, ctrlSearches.onInfoTip)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_SEARCH, onSearch)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		NOTIFY_HANDLER(IDC_RESULTS, NM_CUSTOMDRAW, onCustomDraw)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(SPYFRAME_IGNORETTH_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onIgnoreTTH)
		ALT_MSG_MAP(SPYFRAME_SHOW_NICK)
		MESSAGE_HANDLER(BM_SETCHECK, onShowNick)
		ALT_MSG_MAP(SPYFRAME_LOG_FILE)
		MESSAGE_HANDLER(BM_SETCHECK, onLogToFile)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);

		LRESULT onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (!timer.checkTimerID(wParam))
			{
				bHandled = FALSE;
				return 0;
			}
			if (!isClosedOrShutdown())
				onTimerInternal();
			return 0;
		}

		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			processTasks();
			return 0;
		}

		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/); // !SMT!-S
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
		LRESULT onShowNick(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			showNick = wParam == BST_CHECKED;
			return 0;
		}
		
		LRESULT onLogToFile(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onIgnoreTTH(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			ignoreTTH = wParam == BST_CHECKED;
			return 0;
		}
		
	private:
		enum
		{
			COLUMN_STRING,
			COLUMN_COUNT,
			COLUMN_USERS,
			COLUMN_TIME,
			COLUMN_SHARE_HIT,
			COLUMN_LAST
		};
		
		static const size_t NUM_SEEKERS = 8;
		class ItemInfo
		{
			public:
				ItemInfo(const string& s, uint64_t id);
				bool addSeeker(const string& s);
				void updateNickList();
				tstring getText(uint8_t col) const;
				static int compareItems(const ItemInfo* a, const ItemInfo* b, uint8_t col);
				static int getImageIndex() { return 0; }
				static int getStateImageIndex() { return 0; }

				uint64_t id;
				string key;
				tstring text;
				tstring nickList;
				int count;
				time_t time;
				ClientManagerListener::SearchReply re;

				string seekers[NUM_SEEKERS];
				size_t curPos;
		};

		enum Tasks
		{
			SEARCH
		};
		
		static const int columnId[];
		
		TypedListViewCtrl<ItemInfo, IDC_RESULTS> ctrlSearches;
		CustomDrawHelpers::CustomDrawState customDrawState;
		HTHEME hTheme;
		CStatusBarCtrl ctrlStatus;
		CContainedWindow ignoreTTHContainer;
		CContainedWindow showNickContainer;
		CContainedWindow logToFileContainer;
		CButton ctrlIgnoreTTH;
		CButton ctrlShowNick;
		CButton ctrlLogToFile;
		uint64_t totalCount;
		uint8_t currentSecIndex;
		uint64_t itemId;

		TaskQueue tasks;
		TimerHelper timer;
		
		static const unsigned AVG_TIME = 60;
		uint16_t countPerSec[AVG_TIME];
		
		tstring searchString;

		static HIconWrapper frameIcon;
		COLORREF colorShared, colorSharedLighter;
		
		tstring logFilePath;
		File* logFile;
		string logText;
		
		bool needResort;
		
		bool ignoreTTH;
		bool showNick;
		bool logToFile;
		
		typedef boost::unordered_map<string, ItemInfo*> SpySearchMap;
		
		SpySearchMap searches;
		
		// ClientManagerListener
		struct SearchInfoTask : public Task
		{
			SearchInfoTask(const string& user, const string& s, ClientManagerListener::SearchReply re) : seeker(user), s(s), re(re) {}
			string seeker;
			string s;
			ClientManagerListener::SearchReply re;
		};
		
		// ClientManagerListener
		void on(ClientManagerListener::IncomingSearch, const string& user, const string& s, ClientManagerListener::SearchReply re) noexcept override; // !SMT!-S

		void on(SettingsManagerListener::Repaint) override;

		void openLogFile();
		void closeLogFile();
		void saveLogFile();

		void onTimerInternal();
		void processTasks();
		void addTask(Tasks s, Task* task);
		void removeOldestItem();
};

#endif // !defined(SPY_FRAME_H)

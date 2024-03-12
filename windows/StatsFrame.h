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

#ifndef STATS_FRAME_H
#define STATS_FRAME_H

#ifdef FLYLINKDC_USE_STATS_FRAME

#include "../client/UploadManager.h"
#include "../client/DownloadManager.h"
#include "FlatTabCtrl.h"
#include "StaticFrame.h"
#include "TimerHelper.h"
#include <gdiplustypes.h>

class StatsWindow : public CWindowImpl<StatsWindow>
{
	public:
		StatsWindow();
		~StatsWindow();
		void updateStats();
		void setShowLegend(bool flag) { showLegend = flag; }

		StatsWindow(const StatsWindow&) = delete;
		StatsWindow& operator= (const StatsWindow&) = delete;

		BEGIN_MSG_MAP(StatsWindow)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBackground)
		END_MSG_MAP()

	private:
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 1;
		}

		// Pixels per second
		enum { PIX_PER_SEC = 8 };
		enum { LINE_HEIGHT = 10 };
		
		struct Stat
		{
			int scroll;
			int64_t upload;
			int64_t download;
		};
		typedef deque<Stat> StatList;

		StatList stats;
		int maxItems;

		uint64_t lastTick;
		int64_t maxValue;

		vector<Gdiplus::Point> tmp;
		bool showLegend;
		class BackingStore* backingStore;
};

class StatsFrame : public MDITabChildWindowImpl<StatsFrame>,
	public StaticFrame<StatsFrame, ResourceManager::NETWORK_STATISTICS, IDC_NET_STATS>,
	private TimerHelper
{
	public:
		StatsFrame();

		static CFrameWndClassInfo& GetWndClassInfo();

		typedef MDITabChildWindowImpl<StatsFrame> baseClass;
		BEGIN_MSG_MAP(StatsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_SHOW_LEGEND, onShowLegend)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);

		LRESULT onShowLegend(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}

	private:
		StatsWindow statsWindow;
		SIZE checkBoxSize;
		int checkBoxXOffset;
		int checkBoxYOffset;
		CButton ctrlShowLegend;

		void updateLayout();
};

#endif
#endif // !defined(STATS_FRAME_H)

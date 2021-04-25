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

class StatsFrame : public MDITabChildWindowImpl<StatsFrame>,
	public StaticFrame<StatsFrame, ResourceManager::NETWORK_STATISTICS, IDC_NET_STATS>,
	private TimerHelper
{
	public:
		StatsFrame();
		~StatsFrame() { }

		static CFrameWndClassInfo& GetWndClassInfo();
		
		typedef MDITabChildWindowImpl<StatsFrame> baseClass;
		BEGIN_MSG_MAP(StatsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
	private:
		// Pixels per second
		enum { PIX_PER_SEC = 2 };
		enum { LINE_HEIGHT = 10 };
		
		CBrush m_backgr;
		CPen m_UploadsPen;
		CPen m_DownloadsPen;
		CPen m_foregr;
		
		struct Stat
		{
			Stat(unsigned int aScroll, int64_t aSpeed) : scroll(aScroll), speed(aSpeed) { }
			const unsigned int scroll;
			const int64_t speed;
		};
		typedef deque<Stat> StatList;
		typedef StatList::const_iterator StatIter;

		StatList m_Uploads;
		StatList m_Downloads;
		
		static int g_width;
		static int g_height;
		
		int twidth;
		
		uint64_t lastTick;
		uint64_t scrollTick;
		
		int64_t m_max;
		
		void drawLine(CDC& dc, const StatIter& begin, const StatIter& end, const CRect& rc, const CRect& crc);
		
		void drawLine(CDC& dc, StatList& lst, const CRect& rc, const CRect& crc)
		{
			drawLine(dc, lst.begin(), lst.end(), rc, crc);
		}
		static void findMax(const StatList& lst, int64_t& mspeed)
		{
			for (const auto& val : lst)
				if (val.speed > mspeed) mspeed = val.speed;
		}
		
		static void updateStatList(int64_t bspeed, StatList& lst, int scroll)
		{
			lst.push_front(Stat(scroll, bspeed));
			
			while ((int)lst.size() > ((g_width / PIX_PER_SEC) + 1))
			{
				lst.pop_back();
			}
		}
		
		//static void addTick(int64_t bdiff, uint64_t tdiff, StatList& lst, AvgList& avg, int scroll);
		static void addAproximatedSpeedTick(int64_t bspeed, StatList& lst, int scroll)// [+]IRainman
		{
			updateStatList(bspeed, lst, scroll);
		}
};

#endif
#endif // !defined(STATS_FRAME_H)

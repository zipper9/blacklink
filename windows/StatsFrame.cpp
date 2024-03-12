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

#ifdef FLYLINKDC_USE_STATS_FRAME

#include "StatsFrame.h"
#include "WinUtil.h"
#include "Colors.h"
#include "Fonts.h"
#include "BackingStore.h"
#include "../client/FormatUtil.h"
#include "../client/UploadManager.h"
#include "../client/DownloadManager.h"

#include <gdiplus.h>

static const float PEN_WIDTH = 1.4f;

using namespace Gdiplus;

StatsWindow::StatsWindow() : lastTick(GET_TICK()), maxValue(1024), maxItems(0), showLegend(true), backingStore(nullptr)
{
}

StatsWindow::~StatsWindow()
{
	if (backingStore) backingStore->release();
}

LRESULT StatsWindow::onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (!GetUpdateRect(nullptr))
		return 0;

	CPaintDC paintDC(m_hWnd);

	CRect rc;
	GetClientRect(&rc);
	int width = rc.Width();
	int height = rc.Height();

	if (!backingStore)
	{
		backingStore = BackingStore::getBackingStore();	
		if (!backingStore) return 0;
	}

	HDC hdc = backingStore->getCompatibleDC(paintDC, width, height);
	Graphics* graph = Graphics::FromHDC(hdc);
	graph->SetSmoothingMode(SmoothingModeHighQuality);
	graph->Clear(Color(255, 255, 255));
	
	const int captionHeight = Fonts::g_fontHeight + 2;
	const int lines = (height / (Fonts::g_fontHeight * LINE_HEIGHT)) + 1;
	const int lineHeight = (height - captionHeight) / lines;

	Font font(paintDC, Fonts::g_font);
	Pen pen(Color(0xC0, 0xC0, 0xC0));
	SolidBrush blackBrush(Color(0, 0, 0));
	int y = rc.bottom - 1;
	for (int i = 0; i < lines; ++i)
	{
		y -= lineHeight;
		if (i != lines - 1)
			graph->DrawLine(&pen, rc.left, y, rc.right, y);
				
		const wstring txt = Util::formatBytesW(maxValue * (i + 1) / lines) + L'/' + WSTRING(S);
		graph->DrawString(txt.c_str(), txt.length(), &font, PointF(1, y - captionHeight), &blackBrush);
	}

	int graphHeight = rc.bottom - 1 - y;

	COLORREF color = SETTING(UPLOAD_BAR_COLOR);
	SolidBrush uploadsBrush(Color(128, GetRValue(color), GetGValue(color), GetBValue(color)));
	Pen uploadsPen(Color(GetRValue(color), GetGValue(color), GetBValue(color)), PEN_WIDTH);

	color = SETTING(DOWNLOAD_BAR_COLOR);
	SolidBrush downloadsBrush(Color(128, GetRValue(color), GetGValue(color), GetBValue(color)));
	Pen downloadsPen(Color(GetRValue(color), GetGValue(color), GetBValue(color)), PEN_WIDTH);

	if (!(stats.size() < 2 || maxValue == 0))
	{
		auto i = stats.rbegin();
		int count = 0;
		int x = rc.right - 1;
		int y = (int)(i->upload * graphHeight / maxValue);
		int totalWidth = 0;
		const int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		bool visible = true;
		tmp.clear();
		tmp.push_back(Point(x, rc.bottom - 1));
		tmp.push_back(Point(x, rc.bottom - 1 - y));
		for (++i; i != stats.rend(); ++i)
		{
			dcassert(i->upload <= maxValue);
			y = (int)(i->upload * graphHeight / maxValue);
			x -= i->scroll;
			totalWidth += i->scroll;
			if (visible) tmp.push_back(Point(x, rc.bottom - 1 - y));
			count++;
			if (x < 0)
			{
				visible = false;
				if (totalWidth >= screenWidth)
				{
					maxItems = count + 4;
					break;
				}
			}
		}

		int addPoints = 0;
		if (tmp.back().Y != rc.bottom - 1)
		{
			addPoints++;
			tmp.emplace_back(x, rc.bottom - 1);
		}

		graph->FillPolygon(&uploadsBrush, tmp.data(), tmp.size());
		graph->DrawLines(&uploadsPen, tmp.data() + 1, tmp.size() - (addPoints + 1));

		i = stats.rbegin();
		x = rc.right - 1;
		y = (int)(i->download * graphHeight / maxValue);
		tmp.clear();
		tmp.push_back(Point(x, rc.bottom - 1));
		tmp.push_back(Point(x, rc.bottom - 1 - y));
		for (++i; i != stats.rend(); ++i)
		{
			dcassert(i->download <= maxValue);
			y = (int)(i->download * graphHeight / maxValue);
			x -= i->scroll;
			tmp.push_back(Point(x, rc.bottom - 1 - y));
			if (x < 0) break;
		}

		addPoints = 0;
		if (tmp.back().Y != rc.bottom - 1)
		{
			addPoints++;
			tmp.emplace_back(x, rc.bottom - 1);
		}

		graph->FillPolygon(&downloadsBrush, tmp.data(), tmp.size());
		graph->DrawLines(&downloadsPen, tmp.data() + 1, tmp.size() - (addPoints + 1));
	}

	if (showLegend)
	{
		const wstring& s1 = WSTRING(AVERAGE_DOWNLOAD);
		const wstring& s2 = WSTRING(AVERAGE_UPLOAD);
		RectF r1, r2;
		graph->MeasureString(s1.c_str(), s1.length(), &font, PointF{}, &r1);
		graph->MeasureString(s2.c_str(), s2.length(), &font, PointF{}, &r2);
		int textHeight = std::max(r1.Height, r2.Height);
		int textWidth = std::max(r1.Width, r2.Width);
		int textOffset = textHeight / 2;
		int legendHeight = 5*textHeight;
		int legendWidth = textWidth + 3*textHeight + textOffset;

		SolidBrush backgroundBrush(Color(0xCC, 0xFF, 0xFF, 0xFF));
		static const int offset = 16;
		int y = height - legendHeight - offset;
		Rect r3(offset, y, legendWidth, legendHeight);
		graph->FillRectangle(&backgroundBrush, r3);
		graph->DrawRectangle(&pen, r3);

		y += textHeight;
		Rect r4(offset + textHeight, y, textHeight, textHeight);
		graph->FillRectangle(&downloadsBrush, r4);
		graph->DrawRectangle(&downloadsPen, r4);
		int xtext = offset + 2*textHeight + textOffset;
		graph->DrawString(s1.c_str(), s1.length(), &font, PointF(xtext, y), &blackBrush);

		y += 2*textHeight;
		Rect r5(offset + textHeight, y, textHeight, textHeight);
		graph->FillRectangle(&uploadsBrush, r5);
		graph->DrawRectangle(&uploadsPen, r5);
		graph->DrawString(s2.c_str(), s2.length(), &font, PointF(xtext, y), &blackBrush);
	}

	delete graph;
	BitBlt(paintDC, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
	return 0;
}

void StatsWindow::updateStats()
{
	const uint64_t tick = GET_TICK();
	const uint64_t tdiff = tick - lastTick;
	if (tdiff < 1000)
		return;

	Stat stat;
	stat.download = DownloadManager::getRunningAverage();
	stat.upload = UploadManager::getRunningAverage();
	stat.scroll = (int) (tdiff / 1000) * PIX_PER_SEC;

	stats.push_back(stat);
	if (maxItems)
	{
		while (stats.size() > (size_t) maxItems) stats.pop_front();
	}

	//dcdebug("scroll=%d, ut=%lld, dt=%lld, tdiff=%llu\n", stat.scroll, stat.upload, stat.download, tdiff);

	int64_t mspeed = 0;
	for (const auto& s : stats)
	{
		if (s.upload > mspeed) mspeed = s.upload;
		if (s.download > mspeed) mspeed = s.download;
	}
	if (mspeed > maxValue || (maxValue * 3 / 4) > mspeed)
		maxValue = std::max(mspeed, (int64_t) 1024);
	lastTick = tick;
}

StatsFrame::StatsFrame() : TimerHelper(m_hWnd)
{
	checkBoxSize.cx = checkBoxSize.cy = 0;
	checkBoxXOffset = checkBoxYOffset = 0;
}

LRESULT StatsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	statsWindow.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0/*WS_EX_CLIENTEDGE*/);
	ctrlShowLegend.Create(m_hWnd, rcDefault, CTSTRING(SHOW_LEGEND), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | BS_AUTOCHECKBOX | BS_VCENTER, 0, IDC_SHOW_LEGEND);
	ctrlShowLegend.SetFont(Fonts::g_systemFont);
	ctrlShowLegend.SetCheck(BST_CHECKED);

	createTimer(1000);
	bHandled = FALSE;
	return 1;
}

LRESULT StatsFrame::onShowLegend(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	statsWindow.setShowLegend(ctrlShowLegend.GetCheck() == BST_CHECKED);
	statsWindow.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE);
	return 0;
}

LRESULT StatsFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::NETWORK_STATISTICS, 0);;
	opt->isHub = false;
	return TRUE;
}

LRESULT StatsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	destroyTimer();
	if (!closed)
	{
		closed = true;
		setButtonPressed(IDC_NET_STATS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		bHandled = FALSE;
		return 0;
	}
}

LRESULT StatsFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!checkTimerID(wParam))
	{
		bHandled = FALSE;
		return 0;
	}

	statsWindow.updateStats();
	statsWindow.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE);
	return 0;
}

void StatsFrame::updateLayout()
{
	WINDOWPLACEMENT wp = { sizeof(wp) };
	GetWindowPlacement(&wp);
	int splitBarHeight = wp.showCmd == SW_MAXIMIZE && BOOLSETTING(SHOW_TRANSFERVIEW) ? GetSystemMetrics(SM_CYSIZEFRAME) : 0;
	if (!checkBoxYOffset)
	{
		int xdu, ydu;
		WinUtil::getDialogUnits(m_hWnd, Fonts::g_systemFont, xdu, ydu);
		checkBoxSize.cx = WinUtil::dialogUnitsToPixelsX(12, xdu) + WinUtil::getTextWidth(TSTRING(SHOW_LEGEND), ctrlShowLegend);
		checkBoxSize.cy = WinUtil::dialogUnitsToPixelsY(8, ydu);
		checkBoxXOffset = WinUtil::dialogUnitsToPixelsX(2, xdu);
		checkBoxYOffset = std::max(WinUtil::dialogUnitsToPixelsY(2, ydu), GetSystemMetrics(SM_CYSIZEFRAME));
	}

	int barHeight = checkBoxSize.cy + 2*checkBoxYOffset;
	RECT rect;
	GetClientRect(&rect);
	rect.bottom -= barHeight - splitBarHeight;
	statsWindow.MoveWindow(&rect);

	RECT rc;
	rc.left = checkBoxXOffset;
	rc.top = rect.bottom + checkBoxYOffset;
	rc.right = rc.left + checkBoxSize.cx;
	rc.bottom = rc.top + checkBoxSize.cy;
	ctrlShowLegend.MoveWindow(&rc);
}

LRESULT StatsFrame::onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (wParam != SIZE_MINIMIZED)
	{
		updateLayout();
		Invalidate();
	}
	bHandled = FALSE;
	return 0;
}

CFrameWndClassInfo& StatsFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("StatsFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::NETWORK_STATISTICS, 0);

	return wc;
}

#endif

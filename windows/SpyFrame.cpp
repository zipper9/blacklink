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
#include "Resource.h"
#include "SpyFrame.h"
#include "SearchFrm.h"
#include "MainFrm.h"
#include "WinUtil.h"
#include "../client/ConnectionManager.h"

static const unsigned TIMER_VAL = 1000;

HIconWrapper SpyFrame::frameIcon(IDR_SPY);

int SpyFrame::columnSizes[] =
{
	305,
	70,
	90,
	120,
	20
};

int SpyFrame::columnIndexes[] =
{
	COLUMN_STRING,
	COLUMN_COUNT,
	COLUMN_USERS,
	COLUMN_TIME,
	COLUMN_SHARE_HIT
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::SEARCH_STRING,
	ResourceManager::COUNT,
	ResourceManager::USERS,
	ResourceManager::TIME,
	ResourceManager::SHARED
};

static inline int getColumnSortType(int column)
{
	switch (column)
	{
		case SpyFrame::COLUMN_COUNT:
			return ExListViewCtrl::SORT_INT;
		case SpyFrame::COLUMN_TIME:
		case SpyFrame::COLUMN_SHARE_HIT:
			return ExListViewCtrl::SORT_STRING;
		default:
			return ExListViewCtrl::SORT_STRING_NOCASE;
	}
}

SpyFrame::SpyFrame() :
	timer(m_hWnd), totalCount(0), currentSecIndex(0),
	ignoreTTH(BOOLSETTING(SPY_FRAME_IGNORE_TTH_SEARCHES)),
	showNick(BOOLSETTING(SHOW_SEEKERS_IN_SPY_FRAME)),
	logToFile(BOOLSETTING(LOG_SEEKERS_IN_SPY_FRAME)),
	ignoreTTHContainer(WC_BUTTON, this, SPYFRAME_IGNORETTH_MESSAGE_MAP),
	showNickContainer(WC_BUTTON, this, SPYFRAME_SHOW_NICK),
	logToFileContainer(WC_BUTTON, this, SPYFRAME_LOG_FILE),
	logFile(nullptr), needUpdateTime(true), needResort(false)
{
	memset(countPerSec, 0, sizeof(countPerSec));
	colorShared = RGB(30,213,75);
	colorSharedLighter = HLS_TRANSFORM(colorShared, 35, -20);
	ClientManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
}

LRESULT SpyFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlSearches.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, WS_EX_CLIENTEDGE, IDC_RESULTS);
	ctrlSearches.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	setListViewColors(ctrlSearches);
	WinUtil::setExplorerTheme(ctrlSearches);
	
	ctrlIgnoreTTH.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(IGNORE_TTH_SEARCHES), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlIgnoreTTH.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	ctrlIgnoreTTH.SetFont(Fonts::g_systemFont);
	ctrlIgnoreTTH.SetCheck(ignoreTTH);
	ignoreTTHContainer.SubclassWindow(ctrlIgnoreTTH.m_hWnd);
	
	ctrlShowNick.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(SETTINGS_SHOW_SEEKERS_IN_SPY_FRAME), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlShowNick.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	ctrlShowNick.SetFont(Fonts::g_systemFont);
	ctrlShowNick.SetCheck(showNick);
	showNickContainer.SubclassWindow(ctrlShowNick.m_hWnd);
	
	logFilePath = Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + "SpyLog.log"));
	const tstring logToFileCaption = TSTRING(SETTINGS_LOG_FILE_IN_SPY_FRAME) + _T(" (") + logFilePath + _T(" )");
	
	ctrlLogToFile.Create(ctrlStatus.m_hWnd, rcDefault, logToFileCaption.c_str(), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlLogToFile.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	ctrlLogToFile.SetFont(Fonts::g_systemFont);
	ctrlLogToFile.SetCheck(logToFile);
	logToFileContainer.SubclassWindow(ctrlLogToFile.m_hWnd);
	
	WinUtil::splitTokens(columnIndexes, SETTING(SPY_FRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(SPY_FRAME_WIDTHS), COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_COUNT) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlSearches.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	const int sortColumn = SETTING(SPY_FRAME_SORT);
	ctrlSearches.setSortFromSettings(sortColumn, getColumnSortType(abs(sortColumn)-1), COLUMN_LAST);
	ShareManager::getInstance()->setHits(0);
	
	if (logToFile) openLogFile();

	timer.createTimer(TIMER_VAL);
	ClientManager::g_isSpyFrame = true;
	bHandled = FALSE;
	return 1;
}

void SpyFrame::openLogFile()
{
	if (logFile) return;
	try
	{
		logFile = new File(logFilePath, File::WRITE, File::OPEN | File::CREATE);
		logFile->setEndPos(0);
		if (logFile->getPos() == 0)
			logFile->write("\xef\xbb\xbf");
	}
	catch (const FileException& e)
	{
		LogManager::message("Error creating SearchSpy file " + Text::fromT(logFilePath) + ": " + e.getError());
		delete logFile;
		logFile = nullptr;
	}
}

void SpyFrame::closeLogFile()
{
	if (!logFile) return;
	saveLogFile();
	delete logFile;
	logFile = nullptr;
}

void SpyFrame::saveLogFile()
{
	if (!logFile) return;
	try
	{
		logFile->write(logText);
	}
	catch (const FileException& e)
	{
		LogManager::message("Error writing SearchSpy file: "  + e.getError());
	}
	logText.clear();
}

LRESULT SpyFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ClientManager::g_isSpyFrame = false;
	timer.destroyTimer();
	tasks.setDisabled(true);
	closeLogFile();
	if (!closed)
	{
		closed = true;
		ClientManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		bHandled = TRUE;
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlSearches, SettingsManager::SPY_FRAME_ORDER, SettingsManager::SPY_FRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		
		SET_SETTING(SPY_FRAME_SORT, ctrlSearches.getSortForSettings());
		SET_SETTING(SPY_FRAME_IGNORE_TTH_SEARCHES, ignoreTTH);
		SET_SETTING(SHOW_SEEKERS_IN_SPY_FRAME, showNick);
		SET_SETTING(LOG_SEEKERS_IN_SPY_FRAME, logToFile);
		WinUtil::setButtonPressed(IDC_SEARCH_SPY, false);
		tasks.clear();
		bHandled = FALSE;
		return 0;
	}
}

LRESULT SpyFrame::onColumnClickResults(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
	if (l->iSubItem == ctrlSearches.getSortColumn())
	{
		if (!ctrlSearches.isAscending())
			ctrlSearches.setSort(-1, ctrlSearches.getSortType());
		else
			ctrlSearches.setSortDirection(false);
	}
	else
	{
		ctrlSearches.setSort(l->iSubItem, getColumnSortType(l->iSubItem));
	}
	return 0;
}

void SpyFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[6];
		ctrlStatus.GetClientRect(sr);
		
		const int tmp = sr.Width() > 616 ? 516 : (sr.Width() > 116 ? sr.Width() - 100 : 16);
		
		w[0] = 170;
		w[1] = sr.right - tmp - 150;
		w[2] = w[1] + (tmp - 16) * 1 / 4;
		w[3] = w[1] + (tmp - 16) * 2 / 4;
		w[4] = w[1] + (tmp - 16) * 3 / 4;
		w[5] = w[1] + (tmp - 16) * 4 / 4;
		
		ctrlStatus.SetParts(6, w);
		
		ctrlStatus.GetRect(0, sr);
		ctrlIgnoreTTH.MoveWindow(sr);
		sr.MoveToX(170);
		sr.right += 50;
		ctrlShowNick.MoveWindow(sr);
		sr.MoveToX(sr.right + 10);
		sr.right += 200;
		ctrlLogToFile.MoveWindow(sr);
	}
	
	ctrlSearches.MoveWindow(&rect);
}

void SpyFrame::addTask(Tasks s, Task* task)
{
	bool firstItem;
	uint64_t tick = GET_TICK();
	uint64_t prevTick = tick;
	if (tasks.add(s, task, firstItem, prevTick) && prevTick + TIMER_VAL < tick)
		PostMessage(WM_SPEAKER);
}

void SpyFrame::processTasks()
{
	TaskQueue::List t;
	tasks.get(t);
	if (t.empty()) return;
	
	CLockRedraw<> lockCtrlList(ctrlSearches);
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		switch (i->first)
		{
			case SEARCH:
			{
				SMTSearchInfo* si = (SMTSearchInfo*)i->second;
				if (needUpdateTime)
				{
					currentTime = Text::toT(Util::formatDateTime(GET_TIME()));
					needUpdateTime = false;
				}
				tstring nameList;
				{
					auto& searchItem = searches[si->s];
					if (showNick)
					{
						if (::strncmp(si->seeker.c_str(), "Hub:", 4))
						{
							const string::size_type pos = si->seeker.find(':');
							if (pos != string::npos)
							{
								const string ipStr = si->seeker.substr(0, pos);
								boost::system::error_code ec;
								const auto ip = boost::asio::ip::address_v4::from_string(ipStr, ec);
								if (!ec && !ip.is_unspecified())
								{
									const StringList users = ClientManager::getNicksByIp(ip);
									if (!users.empty())
										si->seeker += " (" + Util::toString(users) + ")";
								}
							}
						}
					}
					if (logFile && logToFile)
					{
						logText += Text::fromT(currentTime) + '\t' +
						           si->seeker + '\t' +
						           si->s + "\r\n";
					}
					if (showNick)
					{
						size_t k;
						for (k = 0; k < NUM_SEEKERS; ++k)
						{
							if (si->seeker == searchItem.seekers[k])
								break; //that user's searching for file already noted
						}
						if (k == NUM_SEEKERS)
							searchItem.addSeeker(si->seeker);
						for (k = 0; k < NUM_SEEKERS; ++k)
						{
							const string::size_type pos = searchItem.seekers[k].find(':');
							if (pos != string::npos)
							{
								nameList += Text::toT(searchItem.seekers[k]);
								const string ip = searchItem.seekers[k].substr(0, pos);
								if (!ip.empty() && ip[0] != 'H')
								{
									IPInfo ipInfo;
									Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_COUNTRY);
									if (ipInfo.countryImage > 0)
									{
										nameList += _T(" [");
										nameList += Text::toT(Util::getCountryShortName(ipInfo.countryImage-1));
										nameList += _T("]");
									}
								}
								nameList += _T("  ");
							}
						}
					}
					++totalCount;
					++countPerSec[currentSecIndex];
				}
				tstring hit;
				if (si->re == ClientManagerListener::SEARCH_PARTIAL_HIT)
					hit = _T('*');
				else if (si->re == ClientManagerListener::SEARCH_HIT)
					hit = _T('+');
				tstring search;
				Text::toT(si->s, search);
				
				const int j = ctrlSearches.find(search);
				if (j == -1)
				{
					TStringList a;
					a.reserve(5);
					a.push_back(search);
					a.push_back(_T("1"));
					a.push_back(nameList);
					a.push_back(currentTime);
					a.push_back(hit);
					ctrlSearches.insert(a, 0, si->re);
					int count = ctrlSearches.GetItemCount();
					if (count > 500)
						ctrlSearches.DeleteItem(--count);
				}
				else
				{
					TCHAR tmp[32];
					tmp[0] = 0;
					ctrlSearches.GetItemText(j, COLUMN_COUNT, tmp, 32);
					ctrlSearches.SetItemText(j, COLUMN_COUNT, Util::toStringW(Util::toInt(tmp) + 1).c_str());
					ctrlSearches.SetItemText(j, COLUMN_USERS, nameList.c_str());
					ctrlSearches.SetItemText(j, COLUMN_TIME, currentTime.c_str());
					ctrlSearches.SetItemText(j, COLUMN_SHARE_HIT, hit.c_str());
					ctrlSearches.SetItem(j, COLUMN_SHARE_HIT, LVIF_PARAM, NULL, 0, 0, 0, si->re);
					if (ctrlSearches.getSortColumn() == COLUMN_COUNT || ctrlSearches.getSortColumn() == COLUMN_TIME)
						needResort = true;
				}
				if (BOOLSETTING(BOLD_SEARCH))
					setDirty();
#ifdef FLYLINKDC_USE_SOUND_AND_POPUP_IN_SEARCH_SPY
				SHOW_POPUP(POPUP_SEARCH_SPY, currentTime + _T(" : ") + nameList + _T("\r\n") + search, TSTRING(SEARCH_SPY));
				PLAY_SOUND(SOUND_SEARCHSPY);
#endif
			}
			break;
		}
		delete i->second;
	}
}

LRESULT SpyFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlSearches && ctrlSearches.GetSelectedCount() == 1)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		CRect rc;
		ctrlSearches.GetHeader().GetWindowRect(&rc);
		if (PtInRect(&rc, pt))
		{
			return 0;
		}

		int i = ctrlSearches.GetNextItem(-1, LVNI_SELECTED);
		if (i != -1)
		{		
			if (pt.x == -1 && pt.y == -1)
				WinUtil::getContextMenuPos(ctrlSearches, pt);
		
			CMenu menu;
			menu.CreatePopupMenu();
			menu.AppendMenu(MF_STRING, IDC_SEARCH, CTSTRING(SEARCH));
		
			searchString = ctrlSearches.ExGetItemTextT(i, COLUMN_STRING);
			menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT SpyFrame::onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	FlatTabOptions* opt = reinterpret_cast<FlatTabOptions*>(lParam);
	opt->icons[0] = opt->icons[1] = frameIcon;
	opt->isHub = false;
	return TRUE;
}

LRESULT SpyFrame::onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (isTTHBase32(Text::fromT(searchString)))
		SearchFrame::openWindow(searchString.substr(4), 0, SIZE_DONTCARE, FILE_TYPE_TTH);
	else
		SearchFrame::openWindow(searchString);
	return 0;
}

void SpyFrame::on(ClientManagerListener::IncomingSearch, const string& user, const string& s, ClientManagerListener::SearchReply re) noexcept
{
	if (ignoreTTH && isTTHBase32(s))
		return;
		
	SMTSearchInfo *x = new SMTSearchInfo(user, s, re);
	std::replace(x->s.begin(), x->s.end(), '$', ' ');
	addTask(SEARCH, x);
}

void SpyFrame::onTimerInternal()
{
	needUpdateTime = true;
	unsigned perMinute = 0;
	for (unsigned i = 0; i < AVG_TIME; ++i)
		perMinute += countPerSec[i];

	unsigned perSecond = perMinute / AVG_TIME;
	if (++currentSecIndex >= AVG_TIME)
		currentSecIndex = 0;
			
	countPerSec[currentSecIndex] = 0;

	size_t hits = ShareManager::getInstance()->getHits();
	TCHAR buf[128];
	_sntprintf(buf, _countof(buf), CTSTRING(SEARCHES_PER), perSecond, perMinute);
	ctrlStatus.SetText(2, (TSTRING(TOTAL) + _T(' ') + Util::toStringW(totalCount)).c_str());
	ctrlStatus.SetText(3, buf);
	ctrlStatus.SetText(4, (TSTRING(HITS) + _T(' ') + Util::toStringW(hits)).c_str());
	const double ratio = totalCount > 0 ? static_cast<double>(hits) / static_cast<double>(totalCount) : 0.0;
	ctrlStatus.SetText(5, (TSTRING(HIT_RATIO) + _T(' ') + Util::toStringW(ratio)).c_str());
	if (needResort)
	{
		needResort = false;
		ctrlSearches.resort();
	}

	if (logFile) saveLogFile();
	processTasks();
}

LRESULT SpyFrame::onLogToFile(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	logToFile = wParam == BST_CHECKED;
	if (logToFile) openLogFile();
		else closeLogFile();
	return 0;
}		

void SpyFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlSearches.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

LRESULT SpyFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW plvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	if (CDDS_PREPAINT == plvcd->nmcd.dwDrawStage)
		return CDRF_NOTIFYITEMDRAW;
		
	if (CDDS_ITEMPREPAINT == plvcd->nmcd.dwDrawStage)
	{
		ClientManagerListener::SearchReply re = (ClientManagerListener::SearchReply)(plvcd->nmcd.lItemlParam);
		
		if (re == ClientManagerListener::SEARCH_HIT)
			plvcd->clrTextBk = colorShared;
		else if (re == ClientManagerListener::SEARCH_PARTIAL_HIT)
			plvcd->clrTextBk = colorSharedLighter;
	}
	return CDRF_DODEFAULT;
}

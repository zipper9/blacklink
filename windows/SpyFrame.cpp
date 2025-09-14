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
#include "SpyFrame.h"
#include "SearchFrm.h"
#include "WinUtil.h"
#include "Fonts.h"
#include "../client/ClientManager.h"
#include "../client/Client.h"
#include "../client/SettingsUtil.h"
#include "../client/FormatUtil.h"
#include "../client/LocationUtil.h"
#include "../client/Util.h"
#include "../client/Tag16.h"
#include "../client/ConfCore.h"

static const unsigned TIMER_VAL = 1000;
static const int MAX_ITEMS = 500;

static const int columnSizes[] =
{
	340,
	50,
	320,
	120,
	120,
	50
};

const int SpyFrame::columnId[] =
{
	COLUMN_STRING,
	COLUMN_COUNT,
	COLUMN_USERS,
	COLUMN_HUB,
	COLUMN_TIME,
	COLUMN_SHARE_HIT
};

static const ResourceManager::Strings columnNames[] =
{
	ResourceManager::SEARCH_STRING,
	ResourceManager::COUNT,
	ResourceManager::USERS,
	ResourceManager::HUB,
	ResourceManager::TIME,
	ResourceManager::SHARED
};

SpyFrame::SpyFrame() :
	timer(m_hWnd), totalCount(0), currentSecIndex(0),
	ignoreTTHContainer(WC_BUTTON, this, SPYFRAME_IGNORETTH_MESSAGE_MAP),
	showNickContainer(WC_BUTTON, this, SPYFRAME_SHOW_NICK),
	logToFileContainer(WC_BUTTON, this, SPYFRAME_LOG_FILE),
	logFile(nullptr), needResort(false),
	hTheme(nullptr),
	itemId(0)
{
	memset(countPerSec, 0, sizeof(countPerSec));
	const auto ss = SettingsManager::instance.getUiSettings();
	ignoreTTH = ss->getBool(Conf::SPY_FRAME_IGNORE_TTH_SEARCHES);
	showNick = ss->getBool(Conf::SHOW_SEEKERS_IN_SPY_FRAME);
	logToFile = ss->getBool(Conf::LOG_SEEKERS_IN_SPY_FRAME);
	colorShared = ss->getInt(Conf::FILE_SHARED_COLOR);
	colorSharedLighter = ColorUtil::lighter(colorShared);
	colorContrastText = RGB(0,0,0);
	ClientManager::getInstance()->addListener(this);
	SettingsManager::instance.addListener(this);

	ctrlSearches.ownsItemData = false;
	ctrlSearches.setColumns(_countof(columnId), columnId, columnNames, columnSizes);
	ctrlSearches.setColumnFormat(COLUMN_COUNT, LVCFMT_RIGHT);
}

SpyFrame::~SpyFrame()
{
	for (auto i = searches.cbegin(); i != searches.cend(); ++i)
		delete i->second;
}

LRESULT SpyFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.ModifyStyleEx(0, WS_EX_COMPOSITED);

	ctrlSearches.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, WS_EX_CLIENTEDGE, IDC_RESULTS);
	ctrlSearches.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	ctrlSearches.SetBkColor(Colors::g_bgColor);
	hTheme = OpenThemeData(m_hWnd, L"EXPLORER::LISTVIEW");
	if (hTheme)
		customDrawState.flags |= CustomDrawHelpers::FLAG_APP_THEMED;
	customDrawState.flags |= CustomDrawHelpers::FLAG_GET_COLFMT;
	
	ctrlIgnoreTTH.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(IGNORE_TTH_SEARCHES), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_AUTOCHECKBOX);
	ctrlIgnoreTTH.SetFont(Fonts::g_systemFont);
	ctrlIgnoreTTH.SetCheck(ignoreTTH);
	ignoreTTHContainer.SubclassWindow(ctrlIgnoreTTH.m_hWnd);

	ctrlShowNick.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(SETTINGS_SHOW_SEEKERS_IN_SPY_FRAME), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_AUTOCHECKBOX);
	ctrlShowNick.SetFont(Fonts::g_systemFont);
	ctrlShowNick.SetCheck(showNick);
	showNickContainer.SubclassWindow(ctrlShowNick.m_hWnd);

	logFilePath = Text::toT(Util::validateFileName(Util::getConfString(Conf::LOG_DIRECTORY) + "SpyLog.log"));
	const tstring logToFileCaption = TSTRING(SETTINGS_LOG_FILE_IN_SPY_FRAME) + _T(" (") + logFilePath + _T(")");

	ctrlLogToFile.Create(ctrlStatus.m_hWnd, rcDefault, logToFileCaption.c_str(), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_AUTOCHECKBOX);
	ctrlLogToFile.SetFont(Fonts::g_systemFont);
	ctrlLogToFile.SetCheck(logToFile);
	logToFileContainer.SubclassWindow(ctrlLogToFile.m_hWnd);

	BOOST_STATIC_ASSERT(_countof(columnSizes) == _countof(SpyFrame::columnId));
	BOOST_STATIC_ASSERT(_countof(columnNames) == _countof(SpyFrame::columnId));

	const auto ss = SettingsManager::instance.getUiSettings();
	ctrlSearches.insertColumns(Conf::SPY_FRAME_ORDER, Conf::SPY_FRAME_WIDTHS, Conf::SPY_FRAME_VISIBLE);
	ctrlSearches.setSortFromSettings(ss->getInt(Conf::SPY_FRAME_SORT));
	ShareManager::getInstance()->setHits(0);

	if (logToFile) openLogFile();

	timer.createTimer(TIMER_VAL);
	ClientManager::searchSpyEnabled = true;
	bHandled = FALSE;
	return 1;
}

LRESULT SpyFrame::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (hTheme)
	{
		CloseThemeData(hTheme);
		hTheme = nullptr;
	}
	bHandled = FALSE;
	return 0;
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
	ClientManager::searchSpyEnabled = false;
	timer.destroyTimer();
	tasks.setDisabled(true);
	closeLogFile();
	if (!closed)
	{
		closed = true;
		ClientManager::getInstance()->removeListener(this);
		SettingsManager::instance.removeListener(this);
		bHandled = TRUE;
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		ctrlSearches.saveHeaderOrder(Conf::SPY_FRAME_ORDER, Conf::SPY_FRAME_WIDTHS, Conf::SPY_FRAME_VISIBLE);
		auto ss = SettingsManager::instance.getUiSettings();
		ss->setInt(Conf::SPY_FRAME_SORT, ctrlSearches.getSortForSettings());
		ss->setBool(Conf::SPY_FRAME_IGNORE_TTH_SEARCHES, ignoreTTH);
		ss->setBool(Conf::SHOW_SEEKERS_IN_SPY_FRAME, showNick);
		ss->setBool(Conf::LOG_SEEKERS_IN_SPY_FRAME, logToFile);
		setButtonPressed(IDC_SEARCH_SPY, false);
		tasks.clear();
		bHandled = FALSE;
		return 0;
	}
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
	
	time_t time = GET_TIME();

	CLockRedraw<> lockCtrlList(ctrlSearches);
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		switch (i->first)
		{
			case SEARCH:
			{
				SearchInfoTask* si = static_cast<SearchInfoTask*>(i->second);
				ItemInfo* ii;
				bool newItem = false;
				auto j = searches.find(si->s);
				if (j == searches.end())
				{
					ii = new ItemInfo(si->s, ++itemId);
					searches.insert(make_pair(si->s, ii));
					newItem = true;
				}
				else
					ii = j->second;
				if (showNick)
				{
					if (::strncmp(si->seeker.c_str(), "Hub:", 4))
					{
						const string::size_type pos = si->seeker.find(':');
						if (pos != string::npos)
						{
							const string ipStr = si->seeker.substr(0, pos);
							IpAddress ip;
							if (Util::parseIpAddress(ip, ipStr) && Util::isValidIp(ip))
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
					logText += Util::formatDateTime(time) + '\t' +
					           si->seeker + '\t' +
					           si->s + "\r\n";
				}
				bool nicksUpdated = false;
				if (showNick && ii->addSeeker(si->seeker, si->hub))
				{
					ii->updateNickList();
					nicksUpdated = true;
				}
				++totalCount;
				++countPerSec[currentSecIndex];

				++ii->count;
				ii->time = time;
				auto prevRe = ii->re;
				if (ii->re == ClientManagerListener::SEARCH_MISS || si->re == ClientManagerListener::SEARCH_HIT)
					ii->re = si->re;
				
				if (newItem)
				{
					ctrlSearches.insertItem(ii, I_IMAGECALLBACK);
					removeOldestItem();
				}
				else
				{
					int sortColumn = ctrlSearches.getRealSortColumn();
					if (sortColumn == COLUMN_COUNT || sortColumn == COLUMN_TIME ||
					    (sortColumn == COLUMN_USERS && nicksUpdated) ||
						(sortColumn == COLUMN_SHARE_HIT && ii->re != prevRe)) needResort = true;
					ctrlSearches.RedrawWindow();
				}

				if (SettingsManager::instance.getUiSettings()->getBool(Conf::BOLD_SEARCH))
					setDirty();
#ifdef FLYLINKDC_USE_SOUND_AND_POPUP_IN_SEARCH_SPY
				SHOW_POPUP(POPUP_SEARCH_SPY, currentTime + _T(" : ") + nickList + _T("\r\n") + search, TSTRING(SEARCH_SPY));
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
		int i = ctrlSearches.GetNextItem(-1, LVNI_SELECTED);
		if (i != -1)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (pt.x == -1 && pt.y == -1)
				WinUtil::getContextMenuPos(ctrlSearches, pt);

			CMenu menu;
			menu.CreatePopupMenu();
			menu.AppendMenu(MF_STRING, IDC_SEARCH, CTSTRING(SEARCH));

			searchString = ctrlSearches.getItemData(i)->text;
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
	opt->icons[0] = opt->icons[1] = g_iconBitmaps.getIcon(IconBitmaps::SEARCH_SPY, 0);
	opt->isHub = false;
	return TRUE;
}

LRESULT SpyFrame::onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (Util::isTTHBase32(Text::fromT(searchString)))
		SearchFrame::openWindow(searchString.substr(4), 0, SIZE_DONTCARE, FILE_TYPE_TTH);
	else
		SearchFrame::openWindow(searchString);
	return 0;
}

void SpyFrame::on(ClientManagerListener::IncomingSearch, int protocol, const string& user, const string& hub, const string& s, ClientManagerListener::SearchReply re) noexcept
{
	if (ignoreTTH && Util::isTTHBase32(s))
		return;
		
	SearchInfoTask *x = new SearchInfoTask(user, hub, s, re);
	if (protocol == ClientBase::TYPE_NMDC)
		std::replace(x->s.begin(), x->s.end(), '$', ' ');
	addTask(SEARCH, x);
}

void SpyFrame::onTimerInternal()
{
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

void SpyFrame::on(SettingsManagerListener::ApplySettings)
{
	if (ctrlSearches.isRedraw())
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

LRESULT SpyFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			CustomDrawHelpers::startDraw(customDrawState, cd);
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
		{
			const ItemInfo* ii = reinterpret_cast<ItemInfo*>(cd->nmcd.lItemlParam);
			cd->clrText = Colors::g_textColor;
			cd->clrTextBk = Colors::g_bgColor;
			if (ii->re == ClientManagerListener::SEARCH_HIT)
			{
				cd->clrTextBk = colorShared;
				cd->clrText = colorContrastText;
			}
			else if (ii->re == ClientManagerListener::SEARCH_PARTIAL_HIT)
			{
				cd->clrTextBk = colorSharedLighter;
				cd->clrText = colorContrastText;
			}
			CustomDrawHelpers::startItemDraw(customDrawState, cd);
			if (hTheme) CustomDrawHelpers::drawBackground(hTheme, customDrawState, cd);
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		}

		case CDDS_ITEMPOSTPAINT:
			CustomDrawHelpers::drawFocusRect(customDrawState, cd);
			return CDRF_SKIPDEFAULT;

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			const ItemInfo* ii = reinterpret_cast<ItemInfo*>(cd->nmcd.lItemlParam);
			int column = ctrlSearches.findColumn(cd->iSubItem);
			if (cd->iSubItem == 0)
				CustomDrawHelpers::drawFirstSubItem(customDrawState, cd, ii->getText(column));
			else
				CustomDrawHelpers::drawTextAndIcon(customDrawState, cd, nullptr, -1, ii->getText(column), false);
			return CDRF_SKIPDEFAULT;
		}
	}
	return CDRF_DODEFAULT;
}

void SpyFrame::removeOldestItem()
{
	int count = ctrlSearches.GetItemCount();
	if (count < MAX_ITEMS) return;
	const ItemInfo* oldest = ctrlSearches.getItemData(0);
	int index = 0;
	for (int i = 1; i < count; ++i)
	{
		const ItemInfo* ii = ctrlSearches.getItemData(i);
		if (ii->id < oldest->id)
		{
			oldest = ii;
			index = i;
		}
	}
	dcdebug("Remove oldest item: %s\n", oldest->key.c_str());
	auto i = searches.find(oldest->key);
	if (i == searches.end())
	{
		dcassert(0);
		return;
	}
	ctrlSearches.DeleteItem(index);
	delete i->second;
	searches.erase(i);
}

SpyFrame::ItemInfo::ItemInfo(const string& s, uint64_t id) :
	id(id), key(s), count(0), re(ClientManagerListener::SEARCH_MISS), curPos(0)
{
	Text::toT(key, text);
}

bool SpyFrame::ItemInfo::addSeeker(const string& user, const string& hub)
{
	for (size_t i = 0; i < NUM_SEEKERS; ++i)
		if (user == seekers[i].user) return false;

	seekers[curPos].user = user;
	seekers[curPos].hub = hub;
	curPos = (curPos + 1) % NUM_SEEKERS;
	return true;
}			

void SpyFrame::ItemInfo::updateNickList()
{
	nickList.clear();
	hubList.clear();
	for (size_t i = 0; i < NUM_SEEKERS; ++i)
	{
		size_t j = (curPos + i) % NUM_SEEKERS;
		const string& user = seekers[j].user;
		const string::size_type pos = user.find(':');
		if (pos != string::npos)
		{
			if (!nickList.empty()) nickList += _T(' ');
			if (pos == 3 && Text::isAsciiPrefix2(user.c_str(), "hub", 3))
			{
				nickList += Text::toT(user.substr(4));
			}
			else
			{
				nickList += Text::toT(user);
				const string ip = user.substr(0, pos);
				IpAddress addr;
				if (!ip.empty() && Util::parseIpAddress(addr, ip) && Util::isValidIp(addr))
				{
					IPInfo ipInfo;
					Util::getIpInfo(addr, ipInfo, IPInfo::FLAG_COUNTRY);
					if (ipInfo.countryCode)
					{
						string s = tagToString(ipInfo.countryCode);
						nickList += _T(" [");
						nickList += Text::toT(s);
						nickList += _T("]");
					}
				}
			}
			string hubName = ClientManager::getOnlineHubName(seekers[j].hub);
			if (!hubName.empty())
			{
				if (!hubList.empty()) hubList += _T(", ");
				hubList += Text::toT(hubName);
			}
		}
	}
}

tstring SpyFrame::ItemInfo::getText(uint8_t col) const
{
	switch (col)
	{
		case COLUMN_STRING:
			return text;
		case COLUMN_COUNT:
			return Util::toStringT(count);
		case COLUMN_USERS:
			return nickList;
		case COLUMN_HUB:
			return hubList;
		case COLUMN_TIME:
			return Text::toT(Util::formatDateTime(time));
		case COLUMN_SHARE_HIT:
			if (re == ClientManagerListener::SEARCH_PARTIAL_HIT) return _T("*");
			if (re == ClientManagerListener::SEARCH_HIT) return _T("+");
			break;
	}
	return Util::emptyStringT;
}

int SpyFrame::ItemInfo::compareItems(const ItemInfo* a, const ItemInfo* b, int col, int /*flags*/)
{
	int result = 0;
	switch (col)
	{
		case COLUMN_STRING:
			return Util::defaultSort(a->text, b->text);
		case COLUMN_COUNT:
			result = compare(a->count, b->count);
			break;
		case COLUMN_USERS:
			result = compare(a->nickList, b->nickList);
			break;
		case COLUMN_HUB:
			result = Util::defaultSort(a->hubList, b->hubList);
			break;
		case COLUMN_TIME:
			result = compare(a->time, b->time);
			break;
		case COLUMN_SHARE_HIT:
			result = compare((int) a->re, (int) b->re);
	}
	if (result) return result;
	return Util::defaultSort(a->text, b->text);
}

CFrameWndClassInfo& SpyFrame::GetWndClassInfo()
{
	static CFrameWndClassInfo wc =
	{
		{
			sizeof(WNDCLASSEX), 0, StartWindowProc,
			0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_3DFACE + 1), NULL, _T("SpyFrame"), NULL
		},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), 0
	};

	if (!wc.m_wc.hIconSm)
		wc.m_wc.hIconSm = wc.m_wc.hIcon = g_iconBitmaps.getIcon(IconBitmaps::SEARCH_SPY, 0);

	return wc;
}

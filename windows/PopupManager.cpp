/*
* Copyright (C) 2003-2005 Pär Björklund, per.bjorklund@gmail.com
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

#include "PopupManager.h"
#include "WinUtil.h"
#include "MainFrm.h"
#include "GdiUtil.h"
#include "Fonts.h"
#include "../client/TimeUtil.h"

#ifdef OSVER_WIN_XP
#include "../client/SysVersion.h"
#endif

static const size_t MAX_POPUPS = 10;

PopupManager::PopupManager() : offset(0), enabled(true)
{
	memset(&lfTitle, 0, sizeof(lfTitle));
	memset(&lfText, 0, sizeof(lfText));
}

void PopupManager::show(const tstring& message, const tstring& title, int icon, bool preview /*= false*/)
{
	if (ClientManager::isBeforeShutdown()) return;
	if (ClientManager::isStartup()) return;
	if (!enabled) return;

	auto mainFrame = MainFrame::getMainFrame();
	if (!mainFrame) return;

	const auto* ss = SettingsManager::instance.getUiSettings();
	if (!preview)
	{
		if (ss->getBool(Conf::POPUP_ONLY_WHEN_AWAY) && !Util::getAway()) return;
		if (ss->getBool(Conf::POPUP_ONLY_WHEN_MINIMIZED) && !MainFrame::isAppMinimized()) return;
	}

	tstring msg = message;
	size_t maxLength = ss->getInt(Conf::POPUP_MAX_LENGTH);
	if (message.length() > maxLength)
	{
		msg.erase(maxLength - 3);
		msg += _T("...");
	}

	int removeTime = ss->getInt(Conf::POPUP_TIME);
	int popupType = ss->getInt(Conf::POPUP_TYPE);
	if (popupType == TYPE_SYSTEM)
	{
		NOTIFYICONDATA nid = {};
		#ifdef OSVER_WIN_XP
		nid.cbSize = SysVersion::isOsVistaPlus() ? sizeof(NOTIFYICONDATA) : NOTIFYICONDATA_V3_SIZE;
		#else
		nid.cbSize = sizeof(NOTIFYICONDATA);
		#endif
		nid.hWnd = mainFrame->m_hWnd;
		nid.uID = 0;
		nid.uFlags = NIF_INFO;
		nid.uTimeout = removeTime * 1000;
		nid.dwInfoFlags = icon;
		_tcsncpy(nid.szInfo, msg.c_str(), 255);
		_tcsncpy(nid.szInfoTitle, title.c_str(), 63);
		Shell_NotifyIcon(NIM_MODIFY, &nid);
		return;
	}

	if (popups.size() > MAX_POPUPS)
	{
		//LogManager::message("PopupManager - popups.size() > 10! Ignore");
		return;
	}

	const string& newTitleFont = ss->getString(Conf::POPUP_TITLE_FONT);
	if (newTitleFont != titleFont || !lfTitle.lfHeight)
	{
		titleFont = newTitleFont;
		Fonts::decodeFont(Text::toT(titleFont.empty() ? getDefaultTitleFont() : titleFont), lfTitle);
	}
	const string& newTextFont = ss->getString(Conf::POPUP_FONT);
	if (newTextFont != textFont || !lfText.lfHeight)
	{
		textFont = newTextFont;
		Fonts::decodeFont(Text::toT(textFont), lfText);
	}

	int dpi = WinUtil::getDisplayDpi();
	int width = ss->getInt(Conf::POPUP_WIDTH) * dpi / 96;
	int height = ss->getInt(Conf::POPUP_HEIGHT) * dpi / 96;

	RECT rcDesktop;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDesktop, 0);
	int screenHeight = rcDesktop.bottom;
	int screenWidth = rcDesktop.right;

	if (offset + height > screenHeight)
		return;

	RECT rc;
	rc.right = screenWidth;
	rc.bottom = screenHeight - offset;
	rc.left = rc.right - width;
	rc.top = rc.bottom - height;

	PopupWindow* p = new PopupWindow;
	p->setText(msg);
	p->setTitle(title);
	p->setTitleFont(lfTitle);
	if (!textFont.empty()) p->setFont(lfText);
	p->setBackgroundColor(ss->getInt(Conf::POPUP_BACKGROUND_COLOR));
	p->setTextColor(ss->getInt(Conf::POPUP_TEXT_COLOR));
	p->setTitleColor(ss->getInt(Conf::POPUP_TITLE_TEXT_COLOR));
	p->setBorderColor(ss->getInt(Conf::POPUP_BORDER_COLOR));
	p->setRemoveTime(Util::getTick() + removeTime * 1000);
	p->setNotifWnd(mainFrame->m_hWnd);
	p->Create(nullptr, &rc);
	popups.push_back(p);

	offset += height;
	p->ShowWindow(SW_SHOWNOACTIVATE);
}

void PopupManager::autoRemove(uint64_t tick)
{
	for (PopupWindow* wnd : popups)
		if (wnd->shouldRemove(tick))
			wnd->hide();
}

void PopupManager::remove(HWND hWnd)
{
	RECT rcRemoved = {};
	for (auto i = popups.begin(); i != popups.end(); ++i)
	{
		PopupWindow* wnd = *i;
		if (wnd->m_hWnd == hWnd)
		{
			wnd->GetWindowRect(&rcRemoved);
			wnd->DestroyWindow();
			delete wnd;
			popups.erase(i);
			break;
		}
	}

	int height = rcRemoved.bottom - rcRemoved.top;
	if (!height) return;

	RECT rc;
	for (PopupWindow* wnd : popups)
	{
		wnd->GetWindowRect(&rc);
		if (rc.top < rcRemoved.top)
		{
			rc.top += height;
			rc.bottom += height;
			wnd->SetWindowPos(nullptr, &rc, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
		}
	}
	offset -= height;
}

void PopupManager::removeAll()
{
	for (PopupWindow* wnd : popups)
	{
		wnd->DestroyWindow();
		delete wnd;
	}
	popups.clear();
	offset = 0;
}

const string& PopupManager::getDefaultTitleFont()
{
	if (defaultTitleFont.empty())
	{
		LOGFONT font;
		int dpi = WinUtil::getDisplayDpi();
		Fonts::decodeFont(Util::emptyStringT, font);
		font.lfHeight = -14 * dpi / 72;
		defaultTitleFont = Text::fromT(Fonts::encodeFont(font));
	}
	return defaultTitleFont;
}

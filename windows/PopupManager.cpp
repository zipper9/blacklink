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

#include "WinUtil.h"
#include "BaseChatFrame.h"
#include "PopupManager.h"
#include "MainFrm.h"

static const size_t MAX_POPUPS = 10;

void PopupManager::Show(const tstring &aMsg, const tstring &aTitle, int icon, bool preview /*= false*/)
{
	dcassert(!ClientManager::isStartup());
	dcassert(!ClientManager::isBeforeShutdown());
	
	if (ClientManager::isBeforeShutdown()) return;
	if (ClientManager::isStartup()) return;
	if (!isActivated) return;		
		
	if (!preview)
	{
		if (BOOLSETTING(POPUP_ONLY_WHEN_AWAY) && !Util::getAway()) return;
		if (BOOLSETTING(POPUP_ONLY_WHEN_MINIMIZED) && !MainFrame::isAppMinimized()) return;
	}

	tstring msg = aMsg;
	size_t maxLength = SETTING(POPUP_MAX_LENGTH);
	if (aMsg.length() > maxLength)
	{
		msg.erase(maxLength - 3);
		msg += _T("...");
	}
#ifdef _DEBUG
	msg += Text::toT("\r\npopups.size() = " + Util::toString(popups.size()));
#endif
	
	if (SETTING(POPUP_TYPE) == BALLOON && MainFrame::getMainFrame())
	{
		NOTIFYICONDATA m_nid = {0};
		m_nid.cbSize = sizeof(NOTIFYICONDATA);
		m_nid.hWnd = MainFrame::getMainFrame()->m_hWnd;
		m_nid.uID = 0;
		m_nid.uFlags = NIF_INFO;
		m_nid.uTimeout = (SETTING(POPUP_TIME) * 1000);
		m_nid.dwInfoFlags = icon;
		_tcsncpy(m_nid.szInfo, msg.c_str(), 255);
		_tcsncpy(m_nid.szInfoTitle, aTitle.c_str(), 63);
		Shell_NotifyIcon(NIM_MODIFY, &m_nid);
		return;
	}
	
	if (popups.size() > MAX_POPUPS)
	{
		//LogManager::message("PopupManager - popups.size() > 10! Ignore");
		return;
	}
	
	if (SETTING(POPUP_TYPE) == CUSTOM && PopupImage != SETTING(POPUP_IMAGE_FILE))
	{
		PopupImage = SETTING(POPUP_IMAGE_FILE);
		popupType = SETTING(POPUP_TYPE);
		m_hBitmap = (HBITMAP)::LoadImage(NULL, Text::toT(PopupImage).c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	}
	
	height = SETTING(POPUP_HEIGHT);
	width = SETTING(POPUP_WIDTH);
	
	CRect rcDesktop;
	
	//get desktop rect so we know where to place the popup
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDesktop, 0);
	
	int screenHeight = rcDesktop.bottom;
	int screenWidth = rcDesktop.right;
	
	//if we have popups all the way up to the top of the screen do not create a new one
	if ((offset + height) > screenHeight)
		return;
		
	//get the handle of the window that has focus
	dcassert(WinUtil::g_mainWnd);
	HWND gotFocus = ::SetFocus(WinUtil::g_mainWnd);
	
	//compute the window position
	CRect rc(screenWidth - width, screenHeight - height - offset, screenWidth, screenHeight - offset);
	
	//Create a new popup
	PopupWnd *p = new PopupWnd(msg, aTitle, rc, id++, m_hBitmap);
	p->height = height; // save the height, for removal
	
	if (SETTING(POPUP_TYPE) != /*CUSTOM*/ BALLOON)
	{
		typedef bool (CALLBACK * LPFUNC)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
		LPFUNC _d_SetLayeredWindowAttributes = (LPFUNC)GetProcAddress(LoadLibrary(_T("user32")), "SetLayeredWindowAttributes");
		if (_d_SetLayeredWindowAttributes)
		{
			p->SetWindowLongPtr(GWL_EXSTYLE, p->GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
			_d_SetLayeredWindowAttributes(p->m_hWnd, 0, SETTING(POPUP_TRANSPARENCY), LWA_ALPHA);
		}
	}
	
	//move the window to the top of the z-order and display it
	p->SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	p->ShowWindow(SW_SHOWNA);
	
	//restore focus to window
	::SetFocus(gotFocus);
	
	//increase offset so we know where to place the next popup
	offset = offset + height;
	
	popups.push_back(p);
}

void PopupManager::on(TimerManagerListener::Second /*type*/, uint64_t tick) noexcept
{
	dcassert(WinUtil::g_mainWnd);
	if (ClientManager::isBeforeShutdown())
		return;
	if (WinUtil::g_mainWnd)
	{
		::PostMessage(WinUtil::g_mainWnd, WM_SPEAKER, MainFrame::REMOVE_POPUP, 0);
	}
}

void PopupManager::AutoRemove()
{
	const uint64_t tick = GET_TICK();
	const uint64_t popupTime = SETTING(POPUP_TIME) * 1000;
	//check all popups and see if we need to remove anyone
	for (auto i = popups.cbegin(); i != popups.cend(); ++i)
	{
		if ((*i)->timeCreated + popupTime < tick)
		{
			//okay remove the first popup
			Remove((*i)->id);
			
			//if list is empty there is nothing more to do
			if (popups.empty())
				return;
				
			//start over from the beginning
			i = popups.cbegin();
		}
	}
}

void PopupManager::Remove(uint32_t pos)
{
	//find the correct window
	auto i = popups.cbegin();
	
	for (; i != popups.cend(); ++i)
	{
		if ((*i)->id == pos)
			break;
	}
	dcassert(i != popups.cend());
	if (i == popups.cend())
		return;
	//remove the window from the list
	PopupWnd *p = *i;
	i = popups.erase(i);
	
	dcassert(p);
	if (p == nullptr)
	{
		return;
	}
	
	//close the window and delete it, ensure that correct height is used from here
	height = p->height;
	p->SendMessage(WM_CLOSE, 0, 0);
	delete p;
	
	//set offset one window position lower
	dcassert(offset > 0);
	offset = offset - height;
	
	//nothing to do
	if (popups.empty())
		return;
	if (!ClientManager::isBeforeShutdown())
	{
		CRect rc;
		//move down all windows
		for (; i != popups.cend(); ++i)
		{
			(*i)->GetWindowRect(rc);
			rc.top += height;
			rc.bottom += height;
			(*i)->MoveWindow(rc);
		}
	}
}

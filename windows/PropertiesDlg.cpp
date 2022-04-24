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

#include "PropertiesDlg.h"

#include "GeneralPage.h"
#include "DownloadPage.h"
#include "PriorityPage.h"
#include "SharePage.h"
#include "SlotPage.h"
#include "AppearancePage.h"
#include "AdvancedPage.h"
#include "LogPage.h"
#include "SoundsPage.h"
#include "UCPage.h"
#include "LimitPage.h"
#include "AVIPreviewPage.h"
#include "OperaColorsPage.h"
#include "ToolbarPage.h"
#include "FavoriteDirsPage.h"
#include "PopupsPage.h"
#include "SDCPage.h"
#include "DefaultClickPage.h"
#include "UserListColors.h"
#include "NetworkPage.h"
#include "ProxyPage.h"
#include "WindowsPage.h"
#include "TabsPage.h"
#include "QueuePage.h"
#include "MiscPage.h"
#include "MessagesPage.h"
#include "RangesPage.h"
#include "RemoteControlPage.h"
#include "WebServerPage.h"
#include "DCLSTPage.h"
#include "IntegrationPage.h"
#include "CertificatesPage.h"
#include "MessagesChatPage.h"
#include "ShareMiscPage.h"
#include "SearchPage.h"
#include "GeoIPPage.h"

#ifdef IRAINMAN_ENABLE_AUTO_BAN
#include "FakeDetectPage.h"
#endif

bool PropertiesDlg::g_needUpdate = false;
bool PropertiesDlg::g_is_create = false;

PropertiesDlg::PropertiesDlg(HWND parent, HICON icon) : TreePropertySheet(CTSTRING(SETTINGS), 0, parent)
{
	this->icon = icon;
	::g_settings = SettingsManager::getInstance();
	g_is_create = true;
	memset(pages, 0, sizeof(pages));
	size_t n = 0;
	pages[n++] = new GeneralPage();
	pages[n++] = new NetworkPage();
	pages[n++] = new ProxyPage();
	pages[n++] = new DownloadPage();
	pages[n++] = new FavoriteDirsPage();
	pages[n++] = new AVIPreview();
	pages[n++] = new QueuePage();
	pages[n++] = new PriorityPage();
	pages[n++] = new SharePage();
	pages[n++] = new SlotPage();
	pages[n++] = new MessagesPage();
	pages[n++] = new AppearancePage();
	pages[n++] = new PropPageTextStyles();
	pages[n++] = new OperaColorsPage();
	pages[n++] = new UserListColors();
	pages[n++] = new Popups();
	pages[n++] = new Sounds();
	pages[n++] = new ToolbarPage();
	pages[n++] = new WindowsPage();
	pages[n++] = new TabsPage();
	pages[n++] = new AdvancedPage();
	pages[n++] = new SDCPage();
	pages[n++] = new GeoIPPage();
	pages[n++] = new DefaultClickPage();
	pages[n++] = new LogPage();
	pages[n++] = new UCPage();
	pages[n++] = new LimitPage();
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	pages[n++] = new FakeDetect();
#endif
	pages[n++] = new CertificatesPage();
	pages[n++] = new MiscPage();
	pages[n++] = new RangesPage();
	pages[n++] = new RemoteControlPage();
	pages[n++] = new WebServerPage();
	pages[n++] = new DCLSTPage();
	pages[n++] = new IntegrationPage();
	pages[n++] = new MessagesChatPage();
	pages[n++] = new ShareMiscPage();
	pages[n++] = new SearchPage();
	
	for (size_t i = 0; i < n; i++)
		AddPage(pages[i]->getPSP());

	// Hide "Apply" button
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	m_psh.dwFlags &= ~PSH_HASHELP;
}

PropertiesDlg::~PropertiesDlg()
{
	for (size_t i = 0; i < numPages; i++)
		delete pages[i];
	g_is_create = false;
}

void PropertiesDlg::onTimerSec()
{
	HWND active = GetActivePage();
	if (!active) return;
	for (size_t i = 0; i < numPages; i++)
	{
		if (!pages[i]) continue;
		HWND page = PropSheet_IndexToHwnd((HWND) *this, i);
		if (page == active)
		{
			pages[i]->onTimer();
			break;
		}
	}
}

void PropertiesDlg::write()
{
	for (size_t i = 0; i < numPages; i++)
	{
		if (!pages[i]) continue;
		HWND page = PropSheet_IndexToHwnd((HWND) *this, i);
		if (page) pages[i]->write();
	}
}

LRESULT PropertiesDlg::onOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	CRect rcWindow;
	if (GetWindowRect(rcWindow))
	{
		write();
	}
	bHandled = FALSE;
	return TRUE;
}

LRESULT PropertiesDlg::onCANCEL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	cancel();
	bHandled = FALSE;
	return TRUE;        //Close Settings window if push 'Cancel'
}

void PropertiesDlg::cancel()
{
	for (size_t i = 0; i < numPages; i++)
	{
		if (!pages[i]) continue;
		HWND page = PropSheet_IndexToHwnd((HWND) * this, i);
		if (page) pages[i]->cancel();
	}
}

int PropertiesDlg::getItemImage(int page) const
{
	return pages[page]? pages[page]->getPageIcon() : 0;
}

void PropertiesDlg::pageChanged(int oldPage, int newPage)
{
	if (oldPage >= 0 && oldPage < numPages && pages[oldPage])
		pages[oldPage]->onHide();
	if (newPage >= 0 && newPage < numPages && pages[newPage])
		pages[newPage]->onShow();
}

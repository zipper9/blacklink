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
#include "LogPage.h"
#include "WinUtil.h"
#include "DialogLayout.h"
#include "BrowseFile.h"
#include "../client/SettingsManager.h"
#include "../client/SettingsUtil.h"
#include "../client/LogManager.h"
#include "../client/PathUtil.h"
#include "../client/File.h"
#include "../client/BusyCounter.h"
#include "../client/ConfCore.h"

static const int LOG_CERT_INDEX = LogManager::LAST;

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { -1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 3,  DialogLayout::SIDE_RIGHT, U_DU(0) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_SETTINGS_LOG_DIR,   FLAG_TRANSLATE, UNSPEC, UNSPEC                       },
	{ IDC_WRITE_LOGS,         FLAG_TRANSLATE, UNSPEC, UNSPEC                       },
	{ IDC_LOG_OPTIONS,        0,              UNSPEC, UNSPEC                       },
	{ IDC_SETTINGS_FILE_NAME, FLAG_TRANSLATE, AUTO,   UNSPEC, 1                    },
	{ IDC_SETTINGS_FORMAT,    FLAG_TRANSLATE, AUTO,   UNSPEC, 1                    },
	{ IDC_LOG_FILE,           0,              UNSPEC, UNSPEC, 0, &align1, &align2  },
	{ IDC_LOG_FORMAT,         0,              UNSPEC, UNSPEC, 0, &align1, &align2  }
};

static const PropPage::Item items[] =
{
	{ IDC_LOG_DIRECTORY, Conf::LOG_DIRECTORY, PropPage::T_STR },
	{ 0,                 0,                   PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ Conf::LOG_MAIN_CHAT,         ResourceManager::SETTINGS_LOG_MAIN_CHAT          },
	{ Conf::LOG_PRIVATE_CHAT,      ResourceManager::SETTINGS_LOG_PRIVATE_CHAT       },
	{ Conf::LOG_DOWNLOADS,         ResourceManager::SETTINGS_LOG_DOWNLOADS          },
	{ Conf::LOG_UPLOADS,           ResourceManager::SETTINGS_LOG_UPLOADS            },
	{ Conf::LOG_SYSTEM,            ResourceManager::SETTINGS_LOG_SYSTEM_MESSAGES    },
	{ Conf::LOG_STATUS_MESSAGES,   ResourceManager::SETTINGS_LOG_STATUS_MESSAGES    },
	{ Conf::LOG_WEBSERVER,         ResourceManager::SETTINGS_LOG_WEBSERVER          },
	{ Conf::LOG_SQLITE_TRACE,      ResourceManager::SETTINGS_LOG_SQLITE_TRACE       },
#ifdef FLYLINKDC_USE_TORRENT
	{ Conf::LOG_TORRENT_TRACE,     ResourceManager::SETTINGS_LOG_TORRENT_TRACE      },
#endif
	{ Conf::LOG_SEARCH_TRACE,      ResourceManager::SETTINGS_LOG_SEARCH_TRACE       },
	{ Conf::LOG_DHT_TRACE,         ResourceManager::SETTINGS_LOG_DHT_TRACE          },
	{ Conf::LOG_PSR_TRACE,         ResourceManager::SETTINGS_LOG_PSR_TRACE          },
	{ Conf::LOG_FLOOD_TRACE,       ResourceManager::SETTINGS_LOG_FLOOD_TRACE        },
	{ Conf::LOG_TCP_MESSAGES,      ResourceManager::SETTINGS_LOG_TCP_MESSAGES       },
	{ Conf::LOG_UDP_PACKETS,       ResourceManager::SETTINGS_LOG_UDP_PACKETS        },
	{ Conf::LOG_TLS_CERTIFICATES,  ResourceManager::SETTINGS_LOG_TLS_CERT           },
	{ Conf::LOG_SOCKET_INFO,       ResourceManager::SETTINGS_LOG_SOCKET_INFO        },
	{ Conf::LOG_IF_SUPPRESS_PMS,   ResourceManager::SETTINGS_LOG_IF_SUPPRESS_PMS    },
	{ 0,                                      ResourceManager::Strings()                       }
};

LRESULT LogPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	BusyCounter<int> busyFlag(initializing);
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_LOG_OPTIONS));

	for (int i = 0; i < LogManager::LAST; ++i)
	{
		TStringPair pair;
		LogManager::getOptions(i, pair);
		options.push_back(pair);
	}

	// LOG_CERT_INDEX
	options.emplace_back(Text::toT(Util::getConfString(Conf::LOG_FILE_TLS_CERT)), Util::emptyStringT);

	logOptions.Attach(GetDlgItem(IDC_LOG_OPTIONS));
	logFile.Attach(GetDlgItem(IDC_LOG_FILE));
	logFormat.Attach(GetDlgItem(IDC_LOG_FORMAT));

	logFile.EnableWindow(FALSE);
	logFormat.EnableWindow(FALSE);
	setEnabled();
	oldSelection = -1;

	return TRUE;
}

void LogPage::setEnabled()
{
	getValues();

	int sel = logOptions.GetSelectedIndex();
	if (sel >= 0 && sel < LogManager::LAST)
	{
		BOOL checkState = logOptions.GetCheckState(sel) != FALSE;
		logFile.EnableWindow(checkState);
		logFormat.EnableWindow(checkState);
		logFile.SetWindowText(options[sel].first.c_str());
		logFormat.SetWindowText(options[sel].second.c_str());
		oldSelection = sel;
	}
	else if (sel == LOG_CERT_INDEX)
	{
		BOOL checkState = logOptions.GetCheckState(sel) != FALSE;
		logFile.EnableWindow(checkState);
		logFormat.EnableWindow(FALSE);
		logFile.SetWindowText(options[sel].first.c_str());
		logFormat.SetWindowText(_T(""));
		oldSelection = sel;
	}
	else
	{
		logFile.EnableWindow(FALSE);
		logFormat.EnableWindow(FALSE);
		logFile.SetWindowText(_T(""));
		logFormat.SetWindowText(_T(""));
	}

	GetDlgItem(IDC_LOG_DIRECTORY).EnableWindow(TRUE);
	GetDlgItem(IDC_BROWSE_LOG).EnableWindow(TRUE);
}

LRESULT LogPage::onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	if (!initializing) setEnabled();
	return 0;
}

void LogPage::getValues()
{
	if (oldSelection >= 0)
	{
		tstring buf;
		WinUtil::getWindowText(logFile, buf);
		if (!buf.empty())
			options[oldSelection].first = std::move(buf);
		if (oldSelection != LOG_CERT_INDEX)
		{
			WinUtil::getWindowText(logFormat, buf);
			if (!buf.empty())
				options[oldSelection].second = std::move(buf);
		}
	}
}

void LogPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_LOG_OPTIONS));

	File::ensureDirectory(Util::getConfString(Conf::LOG_DIRECTORY));

	//make sure we save the last edit too, the user
	//might not have changed the selection
	getValues();

	static const tstring logExt = _T(".log");
	for (int i = 0; i < LogManager::LAST; ++i)
	{
		TStringPair pair = options[i];
		if (!Text::isAsciiSuffix2(pair.first, logExt))
			pair.first += logExt;
		LogManager::setOptions(i, pair);
	}

	// LOG_CERT_INDEX
	Util::setConfString(Conf::LOG_FILE_TLS_CERT, Text::fromT(options[LOG_CERT_INDEX].first));
}

LRESULT LogPage::onClickedBrowseDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir = Text::toT(Util::getConfString(Conf::LOG_DIRECTORY));
	if (WinUtil::browseDirectory(dir, m_hWnd))
	{
		Util::appendPathSeparator(dir);
		SetDlgItemText(IDC_LOG_DIRECTORY, dir.c_str());
	}
	return 0;
}

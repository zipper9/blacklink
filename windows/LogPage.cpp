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

#include "../client/LogManager.h"
#include "../client/File.h"
#include "Resource.h"
#include "LogPage.h"
#include "WinUtil.h"

static const PropPage::TextItem texts[] =
{
	{ IDC_SETTINGS_LOGGING,                     ResourceManager::SETTINGS_LOGGING          },
	{ IDC_SETTINGS_LOG_DIR,                     ResourceManager::DIRECTORY                 },
	{ IDC_SETTINGS_FORMAT,                      ResourceManager::SETTINGS_FORMAT           },
	{ IDC_WRITE_LOGS,                           ResourceManager::SETTINGS_LOGS             },
	{ IDC_SETTINGS_MAX_FINISHED_UPLOADS_L,      ResourceManager::MAX_FINISHED_UPLOADS      },
	{ IDC_SETTINGS_MAX_FINISHED_DOWNLOADS_L,    ResourceManager::MAX_FINISHED_DOWNLOADS    },
	{ IDC_SETTINGS_DB_LOG_FINISHED_UPLOADS_L,   ResourceManager::DB_LOG_FINISHED_UPLOADS   },
	{ IDC_SETTINGS_DB_LOG_FINISHED_DOWNLOADS_L, ResourceManager::DB_LOG_FINISHED_DOWNLOADS },
	{ IDC_SETTINGS_FILE_NAME,                   ResourceManager::SETTINGS_FILE_NAME        },
	{ 0,                                        ResourceManager::Strings()                 }
};

static const PropPage::Item items[] =
{
	{ IDC_LOG_DIRECTORY,                      SettingsManager::LOG_DIRECTORY,             PropPage::T_STR },
	{ IDC_SETTINGS_MAX_FINISHED_UPLOADS,      SettingsManager::MAX_FINISHED_UPLOADS,      PropPage::T_INT },
	{ IDC_SETTINGS_MAX_FINISHED_DOWNLOADS,    SettingsManager::MAX_FINISHED_DOWNLOADS,    PropPage::T_INT },
	{ IDC_SETTINGS_DB_LOG_FINISHED_UPLOADS,   SettingsManager::DB_LOG_FINISHED_UPLOADS,   PropPage::T_INT },
	{ IDC_SETTINGS_DB_LOG_FINISHED_DOWNLOADS, SettingsManager::DB_LOG_FINISHED_DOWNLOADS, PropPage::T_INT },
	{ 0,                                      0,                                          PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ SettingsManager::LOG_MAIN_CHAT,          ResourceManager::SETTINGS_LOG_MAIN_CHAT          },
	{ SettingsManager::LOG_PRIVATE_CHAT,       ResourceManager::SETTINGS_LOG_PRIVATE_CHAT       },
	{ SettingsManager::LOG_DOWNLOADS,          ResourceManager::SETTINGS_LOG_DOWNLOADS          },
	{ SettingsManager::LOG_UPLOADS,            ResourceManager::SETTINGS_LOG_UPLOADS            },
	{ SettingsManager::LOG_SYSTEM,             ResourceManager::SETTINGS_LOG_SYSTEM_MESSAGES    },
	{ SettingsManager::LOG_STATUS_MESSAGES,    ResourceManager::SETTINGS_LOG_STATUS_MESSAGES    },
	{ SettingsManager::LOG_WEBSERVER,          ResourceManager::SETTINGS_LOG_WEBSERVER          },
	{ SettingsManager::LOG_CUSTOM_LOCATION,    ResourceManager::SETTINGS_LOG_CUSTOM_LOCATION    },
	{ SettingsManager::LOG_SQLITE_TRACE,       ResourceManager::SETTINGS_LOG_SQLITE_TRACE       },
	{ SettingsManager::LOG_DDOS_TRACE,         ResourceManager::SETTINGS_LOG_DDOS_TRACE         },
	{ SettingsManager::LOG_COMMAND_TRACE,      ResourceManager::SETTINGS_LOG_COMMAND_TRACE      },
#ifdef FLYLINKDC_USE_TORRENT
	{ SettingsManager::LOG_TORRENT_TRACE,      ResourceManager::SETTINGS_LOG_TORRENT_TRACE      },
#endif
	{ SettingsManager::LOG_PSR_TRACE,          ResourceManager::SETTINGS_LOG_PSR_TRACE          },
	{ SettingsManager::LOG_FLOOD_TRACE,        ResourceManager::SETTINGS_LOG_FLOOD_TRACE        },
	{ SettingsManager::LOG_FILELIST_TRANSFERS, ResourceManager::SETTINGS_LOG_FILELIST_TRANSFERS },
	{ SettingsManager::LOG_IF_SUPPRESS_PMS,    ResourceManager::SETTINGS_LOG_IF_SUPPRESS_PMS    },
	{ 0,                                       ResourceManager::Strings()                       }
};


LRESULT LogPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate(*this, texts);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_LOG_OPTIONS));
	
	for (int i = 0; i < LogManager::LAST; ++i)
	{
		TStringPair pair;
		LogManager::getOptions(i, pair);
		options.push_back(pair);
	}
	
	::EnableWindow(GetDlgItem(IDC_LOG_FORMAT), false);
	::EnableWindow(GetDlgItem(IDC_LOG_FILE), false);
#ifdef FLYLINKDC_USE_ROTATION_FINISHED_MANAGER
	// For fix - crash https://drdump.com/DumpGroup.aspx?DumpGroupID=301739
	::EnableWindow(GetDlgItem(IDC_SETTINGS_MAX_FINISHED_UPLOADS), false);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_MAX_FINISHED_DOWNLOADS), false);
#endif
	setEnabled();
	oldSelection = -1;
	
	return TRUE;
}

void LogPage::setEnabled()
{
	logOptions.Attach(GetDlgItem(IDC_LOG_OPTIONS));
	
	getValues();
	
	int sel = logOptions.GetSelectedIndex();
	
	if (sel >= 0 && sel < LogManager::LAST)
	{
		BOOL checkState = (logOptions.GetCheckState(sel) == BST_CHECKED);
		
		::EnableWindow(GetDlgItem(IDC_LOG_FORMAT), checkState);
		::EnableWindow(GetDlgItem(IDC_LOG_FILE), checkState);
		              
		SetDlgItemText(IDC_LOG_FILE, options[sel].first.c_str());
		SetDlgItemText(IDC_LOG_FORMAT, options[sel].second.c_str());
		
		//save the old selection so we know where to save the values
		oldSelection = sel;
	}
	else
	{
		::EnableWindow(GetDlgItem(IDC_LOG_FORMAT), FALSE);
		::EnableWindow(GetDlgItem(IDC_LOG_FILE), FALSE);
		
		SetDlgItemText(IDC_LOG_FILE, _T(""));
		SetDlgItemText(IDC_LOG_FORMAT, _T(""));
	}
	
	logOptions.Detach();
	::EnableWindow(GetDlgItem(IDC_LOG_DIRECTORY), TRUE);
	::EnableWindow(GetDlgItem(IDC_BROWSE_LOG), TRUE);
}

LRESULT LogPage::onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	setEnabled();
	return 0;
}

void LogPage::getValues()
{
	if (oldSelection >= 0)
	{
		tstring buf;
		WinUtil::getWindowText(GetDlgItem(IDC_LOG_FILE), buf);
		if (!buf.empty())
			options[oldSelection].first = std::move(buf);
		WinUtil::getWindowText(GetDlgItem(IDC_LOG_FORMAT), buf);
		if (!buf.empty())
			options[oldSelection].second = std::move(buf);
	}
}

void LogPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_LOG_OPTIONS));
	
	File::ensureDirectory(SETTING(LOG_DIRECTORY));
	
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
}

LRESULT LogPage::onClickedBrowseDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir = Text::toT(SETTING(LOG_DIRECTORY));
	if (WinUtil::browseDirectory(dir, m_hWnd))
	{
		AppendPathSeparator(dir);
		SetDlgItemText(IDC_LOG_DIRECTORY, dir.c_str());
	}
	return 0;
}

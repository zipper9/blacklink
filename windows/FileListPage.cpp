#include "stdafx.h"
#include "FileListPage.h"
#include "DialogLayout.h"
#include "../client/SettingsManager.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 10, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_SCAN_OPTIONS,         FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_SHOW_SHARED,          FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_SHOW_DOWNLOADED,      FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_SHOW_CANCELED,        FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_SHOW_MY_UPLOADS,      FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_AUTO_MATCH_LISTS,     FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_GENERATION_OPTIONS,   FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_INCLUDE_TIMESTAMP,    FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_INCLUDE_UPLOAD_COUNT, FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_CAPTION_KEEP_LISTS,   FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_SETTINGS_KEEP_LISTS,  0,              UNSPEC, UNSPEC, 0, &align1 }
};

static const PropPage::Item items[] =
{
	{ IDC_SHOW_SHARED,          SettingsManager::FILELIST_SHOW_SHARED,          PropPage::T_BOOL },
	{ IDC_SHOW_DOWNLOADED,      SettingsManager::FILELIST_SHOW_DOWNLOADED,      PropPage::T_BOOL },
	{ IDC_SHOW_CANCELED,        SettingsManager::FILELIST_SHOW_CANCELED,        PropPage::T_BOOL },
	{ IDC_SHOW_MY_UPLOADS,      SettingsManager::FILELIST_SHOW_MY_UPLOADS,      PropPage::T_BOOL },
	{ IDC_AUTO_MATCH_LISTS,     SettingsManager::AUTO_MATCH_DOWNLOADED_LISTS,   PropPage::T_BOOL },
	{ IDC_INCLUDE_TIMESTAMP,    SettingsManager::FILELIST_INCLUDE_TIMESTAMP,    PropPage::T_BOOL },
	{ IDC_INCLUDE_UPLOAD_COUNT, SettingsManager::FILELIST_INCLUDE_UPLOAD_COUNT, PropPage::T_BOOL },
	{ IDC_SETTINGS_KEEP_LISTS,  SettingsManager::KEEP_LISTS_DAYS,               PropPage::T_INT  },
	{ 0,                        0,                                              PropPage::T_END  }
};

LRESULT FileListPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items);
	return TRUE;
}

void FileListPage::write()
{
	PropPage::write(*this, items);
}

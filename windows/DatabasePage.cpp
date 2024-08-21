#include "stdafx.h"
#include "DatabasePage.h"
#include "DialogLayout.h"
#include "UserMessages.h"
#include "../client/SettingsManager.h"
#include "../client/DatabaseManager.h"
#include "../client/ConfCore.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Align align1 = { 1, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align2 = { 3, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align3 = { 4, DialogLayout::SIDE_RIGHT, U_DU(20) };
static const DialogLayout::Align align4 = { 5, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align5 = { 6, DialogLayout::SIDE_RIGHT, U_DU(-8) };
static const DialogLayout::Align align6 = { 8, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align7 = { 9, DialogLayout::SIDE_RIGHT, U_DU(20) };
static const DialogLayout::Align align8 = { 10, DialogLayout::SIDE_RIGHT, U_DU(6) };
static const DialogLayout::Align align9 = { 11, DialogLayout::SIDE_RIGHT, U_DU(-8) };
static const DialogLayout::Align align10 = { 0, DialogLayout::SIDE_LEFT, U_DU(4) };

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_CAPTION_JOURNAL_MODE,      FLAG_TRANSLATE, AUTO,   UNSPEC                       },
	{ IDC_JOURNAL_MODE,              0,              UNSPEC, UNSPEC, 0, &align1           },
	{ IDC_CAPTION_FINISHED_MEM_UP,   FLAG_TRANSLATE, AUTO,   UNSPEC                       },
	{ IDC_FINISHED_MEM_UP,           0,              UNSPEC, UNSPEC, 0, &align2           },
	{ IDC_CAPTION_FINISHED_MEM_DOWN, FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align3           },
	{ IDC_FINISHED_MEM_DOWN,         0,              UNSPEC, UNSPEC, 0, &align4           },
	{ IDC_CAPTION_FINISHED_MEM,      FLAG_TRANSLATE, UNSPEC, UNSPEC, 0, &align10, &align5 },
	{ IDC_CAPTION_FINISHED_DB_UP,    FLAG_TRANSLATE, AUTO,   UNSPEC                       },
	{ IDC_FINISHED_DB_UP,            0,              UNSPEC, UNSPEC, 0, &align6           },
	{ IDC_CAPTION_FINISHED_DB_DOWN,  FLAG_TRANSLATE, AUTO,   UNSPEC, 0, &align7           },
	{ IDC_FINISHED_DB_DOWN,          0,              UNSPEC, UNSPEC, 0, &align8           },
	{ IDC_CAPTION_FINISHED_DB,       FLAG_TRANSLATE, UNSPEC, UNSPEC, 0, &align10, &align9 }
};

static const PropPage::Item items[] =
{
	{ IDC_FINISHED_MEM_UP,   Conf::MAX_FINISHED_UPLOADS,      PropPage::T_INT },
	{ IDC_FINISHED_MEM_DOWN, Conf::MAX_FINISHED_DOWNLOADS,    PropPage::T_INT },
	{ IDC_FINISHED_DB_UP,    Conf::DB_LOG_FINISHED_UPLOADS,   PropPage::T_INT },
	{ IDC_FINISHED_DB_DOWN,  Conf::DB_LOG_FINISHED_DOWNLOADS, PropPage::T_INT },
	{ 0,                     0,                               PropPage::T_END }
};

static const PropPage::ListItem listItems[] =
{
	{ Conf::LOG_FILELIST_TRANSFERS, ResourceManager::SETTINGS_LOG_FILELIST_TRANSFERS },
#ifdef BL_FEATURE_IP_DATABASE
	{ Conf::ENABLE_LAST_IP_AND_MESSAGE_COUNTER, ResourceManager::ENABLE_LAST_IP_AND_MESSAGE_COUNTER },
#endif
	{ Conf::ENABLE_RATIO_USER_LIST, ResourceManager::ENABLE_RATIO_USER_LIST },
	{ Conf::ENABLE_UPLOAD_COUNTER, ResourceManager::ENABLE_UPLOAD_COUNTER },
	{ 0, ResourceManager::Strings() }
};

static const ResourceManager::Strings journalModes[] =
{
	ResourceManager::SETTINGS_DB_JOURNAL_PERSIST,
	ResourceManager::SETTINGS_DB_JOURNAL_WAL,
	ResourceManager::SETTINGS_DB_JOURNAL_MEMORY
};

LRESULT DatabasePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_LIST1));
	ctrlJournal.Attach(GetDlgItem(IDC_JOURNAL_MODE));
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	int value = ss->getInt(Conf::SQLITE_JOURNAL_MODE);
	ss->unlockRead();
	if (!value)
	{
		defaultJournalMode = true;
		value = DatabaseManager::DEFAULT_JOURNAL_MODE;
	}
	else
		defaultJournalMode = false;
	int selIndex = -1;
	for (int i = 0; i < _countof(journalModes); i++)
	{
		ctrlJournal.AddString(CTSTRING_I(journalModes[i]));
		if (i + 1 == value) selIndex = i;
	}
	if (selIndex < 0) selIndex = DatabaseManager::DEFAULT_JOURNAL_MODE - 1;
	ctrlJournal.SetCurSel(selIndex);
	currentJournalMode = selIndex;
	return TRUE;
}

LRESULT DatabasePage::onSetJournalMode(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newSel = ctrlJournal.GetCurSel();
	if (currentJournalMode != newSel)
	{
		currentJournalMode = newSel;
		GetParent().SendMessage(WMU_RESTART_REQUIRED);
	}
	return 0;
}

void DatabasePage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_LIST1));
	int value = ctrlJournal.GetCurSel() + 1;
	if (value == DatabaseManager::DEFAULT_JOURNAL_MODE && defaultJournalMode) value = 0;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	ss->setInt(Conf::SQLITE_JOURNAL_MODE, value);
	ss->unlockWrite();
}

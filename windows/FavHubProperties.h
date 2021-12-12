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

#ifndef FAV_HUB_PROPERTIES_H_
#define FAV_HUB_PROPERTIES_H_

#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include "resource.h"
#include "ShareGroupList.h"

class FavoriteHubEntry;

class FavoriteHubTabName : public CDialogImpl<FavoriteHubTabName>
{
	public:
		enum { IDD = IDD_FAVORITE_HUB_TAB1 };

		FavoriteHubTabName(FavoriteHubEntry* entry) : entry(entry), addressChanged(false) {}
		
		BEGIN_MSG_MAP(FavoriteHubTabName)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_HUBADDR, EN_CHANGE, onTextChanged)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onTextChanged(WORD, WORD wID, HWND hWndCtl, BOOL&);

		CEdit ctrlName;
		CEdit ctrlDesc;
		CEdit ctrlAddress;
		CEdit ctrlKeyPrint;
		CComboBox ctrlGroup;
		CButton ctrlPreferIP6;
		FavoriteHubEntry* entry;
		bool addressChanged;
};

class FavoriteHubTabIdent : public CDialogImpl<FavoriteHubTabIdent>
{
	public:
		enum { IDD = IDD_FAVORITE_HUB_TAB2 };

		FavoriteHubTabIdent(FavoriteHubEntry* entry) : entry(entry) {}

		BEGIN_MSG_MAP(FavoriteHubTabIdent)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_CLIENT_ID, onChangeClientId);
		COMMAND_ID_HANDLER(IDC_WIZARD_NICK_RND, onRandomNick);
		COMMAND_HANDLER(IDC_HUBNICK, EN_CHANGE, onTextChanged)
		COMMAND_HANDLER(IDC_HUBPASS, EN_CHANGE, onTextChanged)
		COMMAND_HANDLER(IDC_HUBUSERDESCR, EN_CHANGE, onTextChanged)
		COMMAND_HANDLER(IDC_HUBEMAIL, EN_CHANGE, onTextChanged)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeClientId(WORD, WORD, HWND, BOOL&);
		LRESULT onRandomNick(WORD, WORD, HWND, BOOL&);
		LRESULT onTextChanged(WORD, WORD wID, HWND hWndCtl, BOOL&);

		FavoriteHubEntry* entry;

		CEdit ctrlNick;
		CEdit ctrlPassword;
		CEdit ctrlDesc;
		CEdit ctrlEmail;
		CEdit ctrlAwayMsg;
		CComboBox ctrlShareGroup;
		CComboBox ctrlClientId;

		ShareGroupList shareGroups;
};

class FavoriteHubTabOptions : public CDialogImpl<FavoriteHubTabOptions>
{
	public:
		enum { IDD = IDD_FAVORITE_HUB_TAB3 };

		FavoriteHubTabOptions(FavoriteHubEntry* entry) : entry(entry) {}

		BEGIN_MSG_MAP(FavoriteHubTabOptions)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_OVERRIDE_DEFAULT, onChangeSearchCheck);
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeSearchCheck(WORD, WORD, HWND, BOOL&);

		CComboBox ctrlEncoding;
		CComboBox ctrlConnType;
		CEdit ctrlIpAddress;
		CButton ctrlExclChecks;
		CButton ctrlShowJoins;
		CButton ctrlSuppressMsg;
		CButton ctrlSearchOverride;
		CEdit ctrlSearchActive;
		CEdit ctrlSearchPassive;
		FavoriteHubEntry* entry;
};

class FavoriteHubTabCheats : public CDialogImpl<FavoriteHubTabCheats>
{
	public:
		enum { IDD = IDD_FAVORITE_HUB_TAB5 };

		FavoriteHubTabCheats(FavoriteHubEntry* entry) : entry(entry) {}

		BEGIN_MSG_MAP(FavoriteHubTabCheats)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_FAKE_SHARE, onChangeFakeShare);
		COMMAND_ID_HANDLER(IDC_OVERRIDE_STATUS, onChangeOverrideStatus);
		COMMAND_ID_HANDLER(IDC_WIZARD_NICK_RND, onRandomFileCount);
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeFakeShare(WORD, WORD, HWND, BOOL&);
		LRESULT onChangeOverrideStatus(WORD, WORD, HWND, BOOL&);
		LRESULT onRandomFileCount(WORD, WORD, HWND, BOOL&);

		CButton ctrlFakeHubCount;
		CButton ctrlEnableFakeShare;
		CEdit ctrlFakeShare;
		CComboBox ctrlFakeShareUnit;
		CEdit ctrlFakeCount;
		CButton ctrlRandomCount;
		CButton ctrlOverrideStatus;
		CComboBox ctrlFakeStatus;
		FavoriteHubEntry* entry;
};

class FavoriteHubTabAdvanced : public CDialogImpl<FavoriteHubTabAdvanced>
{
	public:
		enum { IDD = IDD_FAVORITE_HUB_TAB4 };

		FavoriteHubTabAdvanced(FavoriteHubEntry* entry) : entry(entry) {}
		
		BEGIN_MSG_MAP(FavoriteHubTabAdvanced)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);

		CEdit ctrlRaw[5];
		CEdit ctrlOpChat;
		FavoriteHubEntry* entry;
};

class FavHubProperties : public CDialogImpl<FavHubProperties>
{
	public:
		enum { IDD = IDD_FAVORITE_HUB };

		FavHubProperties(FavoriteHubEntry* entry) : entry(entry),
			tabName(entry), tabIdent(entry), tabOptions(entry), tabCheats(entry), tabAdvanced(entry) {}

		BEGIN_MSG_MAP_EX(FavHubProperties)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, onChangeTab)
		COMMAND_ID_HANDLER(IDOK, onClose)
		COMMAND_ID_HANDLER(IDCANCEL, onClose)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeTab(int idCtrl, LPNMHDR pnmh, BOOL& bHandled) { changeTab(); return 1; }
		LRESULT onClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
	private:
		FavoriteHubEntry* entry;
		CTabCtrl ctrlTabs;
		FavoriteHubTabName tabName;
		FavoriteHubTabIdent tabIdent;
		FavoriteHubTabOptions tabOptions;
		FavoriteHubTabCheats tabCheats;
		FavoriteHubTabAdvanced tabAdvanced;

		void changeTab();
};

#endif // FAV_HUB_PROPERTIES_H_

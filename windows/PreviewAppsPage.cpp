/*
* Copyright (C) 2003 Twink,  spm7@waikato.ac.nz
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
#include "../client/SettingsManager.h"
#include "PreviewAppsPage.h"
#include "PreviewDlg.h"
#include "PreviewMenu.h"
#include "WinUtil.h"

#ifdef OSVER_WINXP
#include "../client/CompatibilityManager.h"
#endif

static const WinUtil::TextItem texts[] =
{
	{ IDC_ADD_MENU,    ResourceManager::ADD                     },
	{ IDC_CHANGE_MENU, ResourceManager::EDIT_ACCEL              },
	{ IDC_REMOVE_MENU, ResourceManager::REMOVE                  },
	{ IDC_DETECT,      ResourceManager::SETTINGS_PREVIEW_DETECT },
	{ 0,               ResourceManager::Strings()               }
};

class DetectedAppsDlg : public CDialogImpl<DetectedAppsDlg>, public CDialogResize<DetectedAppsDlg>
{
	public:
		enum { IDD = IDD_DETECTED_APPS };

		DetectedAppsDlg(PreviewApplication::List& appList)
		{
			this->appList = std::move(appList);
			appList.clear();
		}
		~DetectedAppsDlg()
		{
			PreviewApplication::clearList(appList);
		}
		
		BEGIN_MSG_MAP(DetectedAppsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		CHAIN_MSG_MAP(CDialogResize<DetectedAppsDlg>)
		END_MSG_MAP()

		BEGIN_DLGRESIZE_MAP(DetectedAppsDlg)
		BEGIN_DLGRESIZE_GROUP()
		DLGRESIZE_CONTROL(IDC_LIST1, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		END_DLGRESIZE_GROUP()
		DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		END_DLGRESIZE_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);

		void getSelectedApps(PreviewApplication::List& selected)
		{
			selected = std::move(appList);
			appList.clear();
		}

	private:
		CListViewCtrl ctrlList;
		PreviewApplication::List appList;
};

static const WinUtil::TextItem texts2[] =
{
	{ IDOK,     ResourceManager::OK        },
	{ IDCANCEL, ResourceManager::CANCEL    },
	{ 0,        ResourceManager::Strings() }
};

LRESULT DetectedAppsDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DlgResize_Init();
	SetWindowText(CTSTRING(SETTINGS_PREVIEW_DETECTED));
	WinUtil::translate(*this, texts2);

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::PREVIEW, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	ctrlList.Attach(GetDlgItem(IDC_LIST1));
	ctrlList.SetExtendedListViewStyle(WinUtil::getListViewExStyle(true));
	WinUtil::setExplorerTheme(ctrlList);

	CRect rc;
	ctrlList.GetClientRect(rc);
	ctrlList.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);

	int count = 0;
	for (PreviewApplication* app : appList)
	{
		ctrlList.InsertItem(count, Text::toT(app->name).c_str());
		ctrlList.SetCheckState(count, TRUE);
		count++;
	}

	CenterWindow(GetParent());
	return 0;
}

LRESULT DetectedAppsDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		PreviewApplication::List selected;
		for (size_t i = 0; i < appList.size(); i++)
		{
			if (ctrlList.GetCheckState(i))
				selected.push_back(appList[i]);
			else
				delete appList[i];
		}
		appList = std::move(selected);
	}
	else
		PreviewApplication::clearList(appList);		
	EndDialog(wID);
	return 0;
}

LRESULT DetectedAppsDlg::onSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CDialogResize<DetectedAppsDlg>::OnSize(uMsg, wParam, lParam, bHandled);
	CRect rc;
	ctrlList.GetClientRect(rc);
	ctrlList.SetColumnWidth(0, rc.Width());
	return 0;
}

LRESULT PreviewAppsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	WinUtil::translate(*this, texts);
#ifdef OSVER_WINXP
	if (CompatibilityManager::isOsVistaPlus())
#endif
		CButton(GetDlgItem(IDC_DETECT)).SetBitmap(g_iconBitmaps.getBitmap(IconBitmaps::SEARCH, 0));

	CRect rc;
	ctrlCommands.Attach(GetDlgItem(IDC_MENU_ITEMS));
	ctrlCommands.GetClientRect(rc);

	ctrlCommands.InsertColumn(0, CTSTRING(SETTINGS_NAME), LVCFMT_LEFT, rc.Width() / 5, 0);
	ctrlCommands.InsertColumn(1, CTSTRING(SETTINGS_COMMAND), LVCFMT_LEFT, rc.Width() * 2 / 5, 1);
	ctrlCommands.InsertColumn(2, CTSTRING(SETTINGS_ARGUMENT), LVCFMT_LEFT, rc.Width() / 5, 2);
	ctrlCommands.InsertColumn(3, CTSTRING(SETTINGS_EXTENSIONS), LVCFMT_LEFT, rc.Width() / 5, 3);

	ctrlCommands.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlCommands);

	insertAppList();
	return 0;
}

void PreviewAppsPage::insertAppList()
{
	const auto& lst = FavoriteManager::getInstance()->getPreviewApps();
	int cnt = ctrlCommands.GetItemCount();
	for (const PreviewApplication* app : lst)
		addEntry(app, cnt++);
	checkMenu();
}

void PreviewAppsPage::addEntry(const PreviewApplication* pa, int pos)
{
	TStringList lst;
	lst.push_back(Text::toT(pa->name));
	lst.push_back(Text::toT(pa->application));
	lst.push_back(Text::toT(pa->arguments));
	lst.push_back(Text::toT(pa->extension));
	ctrlCommands.insert(pos, lst, 0, 0);
}

void PreviewAppsPage::checkMenu()
{
	BOOL enable = (ctrlCommands.GetItemCount() > 0 && ctrlCommands.GetSelectedCount() == 1) ? TRUE : FALSE;
	GetDlgItem(IDC_CHANGE_MENU).EnableWindow(enable);
	GetDlgItem(IDC_REMOVE_MENU).EnableWindow(enable);
}

LRESULT PreviewAppsPage::onAddMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PreviewDlg dlg(true);
	if (dlg.DoModal() == IDOK)
	{
		addEntry(FavoriteManager::getInstance()->addPreviewApp(
			Text::fromT(dlg.name),
		    Text::fromT(dlg.application),
		    Text::fromT(dlg.arguments),
		    Text::fromT(dlg.extensions)), ctrlCommands.GetItemCount());
	}
	checkMenu();
	return 0;
}

LRESULT PreviewAppsPage::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	checkMenu();
	return 0;
}

LRESULT PreviewAppsPage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD_MENU, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE_MENU, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT PreviewAppsPage::onChangeMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int sel = ctrlCommands.GetSelectedIndex();
		PreviewApplication* pa = FavoriteManager::getInstance()->getPreviewApp(sel);
		if (pa)
		{
			PreviewDlg dlg(false);
			dlg.name = Text::toT(pa->name);
			dlg.application = Text::toT(pa->application);
			dlg.arguments = Text::toT(pa->arguments);
			dlg.extensions = Text::toT(pa->extension);

			if (dlg.DoModal() == IDOK)
			{
				pa->name = Text::fromT(dlg.name);
				pa->application = Text::fromT(dlg.application);
				pa->arguments = Text::fromT(dlg.arguments);
				pa->extension = Text::fromT(dlg.extensions);

				ctrlCommands.SetItemText(sel, 0, dlg.name.c_str());
				ctrlCommands.SetItemText(sel, 1, dlg.application.c_str());
				ctrlCommands.SetItemText(sel, 2, dlg.arguments.c_str());
				ctrlCommands.SetItemText(sel, 3, dlg.extensions.c_str());
			}
		}
	}

	return 0;
}

LRESULT PreviewAppsPage::onRemoveMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int sel = ctrlCommands.GetSelectedIndex();
		FavoriteManager::getInstance()->removePreviewApp(sel);
		ctrlCommands.DeleteItem(sel);
	}
	checkMenu();
	return 0;
}

LRESULT PreviewAppsPage::onDetect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PreviewApplication::List appList;
	PreviewMenu::detectApps(appList);
	if (appList.empty())
	{
		MessageBox(CTSTRING(SETTINGS_PREVIEW_NOT_DETECTED), getAppNameVerT().c_str(), MB_OK | MB_ICONINFORMATION);
		return 0;
	}
	DetectedAppsDlg dlg(appList);
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		dlg.getSelectedApps(appList);
		FavoriteManager::getInstance()->addPreviewApps(appList, true);
		ctrlCommands.DeleteAllItems();
		insertAppList();
	}
	return 0;
}

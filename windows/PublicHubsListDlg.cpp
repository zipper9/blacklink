#include "stdafx.h"
#include "PublicHubsListDlg.h"
#include "LineDlg.h"
#include "WinUtil.h"
#include "../client/SettingsManager.h"

static const WinUtil::TextItem texts[] =
{
	{ IDC_LIST_ADD,    ResourceManager::ADD        },
	{ IDC_LIST_EDIT,   ResourceManager::EDIT_ACCEL },
	{ IDC_LIST_REMOVE, ResourceManager::REMOVE     },
	{ IDC_LIST_UP,     ResourceManager::MOVE_UP    },
	{ IDC_LIST_DOWN,   ResourceManager::MOVE_DOWN  },
	{ IDOK,            ResourceManager::OK         },
	{ IDCANCEL,        ResourceManager::CANCEL     },
	{ 0,               ResourceManager::Strings()  }
};

LRESULT PublicHubsListDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/)
{
	SetWindowText(CTSTRING(CONFIGURED_HUB_LISTS));
	WinUtil::translate(m_hWnd, texts);

	CRect rc;
	ctrlList.Attach(GetDlgItem(IDC_LIST_LIST));
	ctrlList.SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	WinUtil::setExplorerTheme(ctrlList);
	ctrlList.GetClientRect(rc);
	ctrlList.InsertColumn(0, CTSTRING(NAME), LVCFMT_LEFT, rc.Width() - 4, 0);
	ctrlList.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);

	int index = 0;
	for (auto i = hubLists.cbegin(); i != hubLists.cend(); ++i)
		ctrlList.insert(index++, Text::toT(i->url));

	updateButtonState(FALSE);

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::INTERNET_HUBS, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);
	
	CenterWindow(GetParent());
	return 0;
}

LRESULT PublicHubsListDlg::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled)
{
	LineDlg dlg;
	dlg.title = CTSTRING(HUB_LIST);
	dlg.description = CTSTRING(HUB_LIST_ADD);
	dlg.icon = IconBitmaps::INTERNET_HUBS;
	dlg.allowEmpty = false;
	if (dlg.DoModal(m_hWnd) == IDOK)
		ctrlList.insert(0, dlg.line);
	return 0;
}

LRESULT PublicHubsListDlg::onMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled)
{
	int j = ctrlList.GetItemCount();
	for (int i = 1; i < j; ++i)
	{
		if (ctrlList.GetItemState(i, LVIS_SELECTED))
		{
			ctrlList.moveItem(i, i - 1);
			ctrlList.SelectItem(i - 1);
			ctrlList.SetFocus();
		}
	}
	return 0;
}

LRESULT PublicHubsListDlg::onMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled)
{
	int j = ctrlList.GetItemCount() - 2;
	for (int i = j; i >= 0; --i)
	{
		if (ctrlList.GetItemState(i, LVIS_SELECTED))
		{
			ctrlList.moveItem(i, i + 1);
			ctrlList.SelectItem(i + 1);
			ctrlList.SetFocus();
		}
	}
	return 0;
}

LRESULT PublicHubsListDlg::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled)
{
	int i = -1;
	TCHAR buf[256];
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		LineDlg dlg;
		dlg.title = CTSTRING(HUB_LIST);
		dlg.description = CTSTRING(HUB_LIST_EDIT);
		ctrlList.GetItemText(i, 0, buf, 256);
		dlg.line = buf;
		dlg.icon = IconBitmaps::INTERNET_HUBS;
		dlg.allowEmpty = false;
		if (dlg.DoModal(m_hWnd) == IDOK)
			ctrlList.SetItemText(i, 0, dlg.line.c_str());
	}
	return 0;
}

LRESULT PublicHubsListDlg::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL &bHandled)
{
	if (MessageBox(CTSTRING(REALLY_REMOVE), getAppNameVerT().c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
	int i = -1;
	while ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
		ctrlList.DeleteItem(i);
	updateButtonState(FALSE);
	return 0;
}

LRESULT PublicHubsListDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
{
	if (wID == IDOK)
	{
		TCHAR buf[512];
		string tmp;
		int count = ctrlList.GetItemCount();
		for (int i = 0; i < count; i++)
		{
			ctrlList.GetItemText(i, 0, buf, 512);
			if (!tmp.empty()) tmp += ';';
			tmp += Text::fromT(buf);
		}
		SettingsManager::getInstance()->set(SettingsManager::HUBLIST_SERVERS, tmp);
	}
	EndDialog(wID);
	return 0;
}

LRESULT PublicHubsListDlg::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL & /*bHandled*/)
{
	NM_LISTVIEW* lv = reinterpret_cast<NM_LISTVIEW*>(pnmh);
	BOOL state = (lv->uNewState & LVIS_FOCUSED) != 0;
	updateButtonState(state);
	return 0;
}

void PublicHubsListDlg::updateButtonState(BOOL state)
{
	GetDlgItem(IDC_LIST_UP).EnableWindow(state);
	GetDlgItem(IDC_LIST_DOWN).EnableWindow(state);
	GetDlgItem(IDC_LIST_EDIT).EnableWindow(state);
	GetDlgItem(IDC_LIST_REMOVE).EnableWindow(state);
}

LRESULT PublicHubsListDlg::onDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = reinterpret_cast<NMITEMACTIVATE*>(pnmh);
	if (item->iItem >= 0)
	{
		PostMessage(WM_COMMAND, IDC_LIST_EDIT, 0);
	}
	else if (item->iItem == -1)
	{
		PostMessage(WM_COMMAND, IDC_LIST_ADD, 0);
	}
	return 0;
}

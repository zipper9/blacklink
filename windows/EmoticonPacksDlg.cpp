#include "stdafx.h"

#ifdef BL_UI_FEATURE_EMOTICONS

#include "EmoticonPacksDlg.h"
#include "DialogLayout.h"
#include "ImageLists.h"
#include "WinUtil.h"
#include "../client/File.h"
#include "../client/Text.h"
#include "../client/AppPaths.h"
#include "../client/BusyCounter.h"

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Item layoutItems[] =
{
	{ IDC_ENABLE, FLAG_TRANSLATE, AUTO, UNSPEC },
	{ IDC_LIST_UP, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_LIST_DOWN, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDOK, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDCANCEL, FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

LRESULT EmoticonPacksDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(SELECT_EMOTICON_PACKS));
	HICON icon = g_iconBitmaps.getIcon(IconBitmaps::EDITOR_EMOTICON, 0);
	SetIcon(icon, FALSE);
	SetIcon(icon, TRUE);

	DialogLayout::layout(m_hWnd, layoutItems, _countof(layoutItems));

	ctrlList = GetDlgItem(IDC_LIST1);
	ctrlEnable = GetDlgItem(IDC_ENABLE);
	ctrlUp = GetDlgItem(IDC_LIST_UP);
	ctrlDown = GetDlgItem(IDC_LIST_DOWN);

	vector<Item> newItems;
	for (const Item& item : items)
		for (auto i = availPacks.begin(); i != availPacks.end(); ++i)
			if (stricmp(*i, item.name) == 0)
			{
				newItems.push_back(item);
				availPacks.erase(i);
				break;
			}
	for (const tstring& name : availPacks)
		newItems.emplace_back(Item{name, false});
	items = std::move(newItems);
	availPacks.clear();

	ctrlList.SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_CHECKBOXES);
	WinUtil::setExplorerTheme(ctrlList);
	CRect rc;
	ctrlList.GetClientRect(rc);
	ctrlList.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
	ctrlList.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);

	BusyCounter<int> busyFlag(initList);
	for (size_t i = 0; i < items.size(); i++)
	{
		ctrlList.InsertItem(i, items[i].name.c_str());
		ctrlList.SetCheckState(i, items[i].enabled);
	}

	ctrlEnable.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
	ctrlList.EnableWindow((BOOL) enabled);
	updateButtons((BOOL) enabled);

	CenterWindow(GetParent());
	return 0;
}

LRESULT EmoticonPacksDlg::onCloseCmd(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	enabled = ctrlEnable.GetCheck() == BST_CHECKED;
	EndDialog(wID);
	return 0;
}

LRESULT EmoticonPacksDlg::onEnable(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	BOOL enable = ctrlEnable.GetCheck() == BST_CHECKED;
	ctrlList.EnableWindow(enable);
	updateButtons(enable);
	return 0;
}

LRESULT EmoticonPacksDlg::onListItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (initList) return 0;
	const NMLISTVIEW* l = reinterpret_cast<const NMLISTVIEW*>(pnmh);
	if (l->uChanged & LVIF_STATE)
	{
		int index = l->iItem;
		if (index >= 0 && (unsigned) index < items.size())
			items[index].enabled = ctrlList.GetCheckState(index) == BST_CHECKED;
	}
	updateButtons(TRUE);
	return 0;
}

void EmoticonPacksDlg::updateButtons(BOOL enable)
{
	if (!enable)
	{
		ctrlUp.EnableWindow(FALSE);
		ctrlDown.EnableWindow(FALSE);
		return;
	}
	int index = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	ctrlUp.EnableWindow(index > 0);
	ctrlDown.EnableWindow(index != -1 && index + 1 < ctrlList.GetItemCount());
}

void EmoticonPacksDlg::moveSelection(int direction)
{
	int index = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	if (index < 0) return;
	int otherIndex = index + direction;
	int selIndex = otherIndex;
	if (otherIndex < 0 || otherIndex >= ctrlList.GetItemCount()) return;
	std::swap(items[index], items[otherIndex]);
	if (otherIndex < index) std::swap(otherIndex, index);
	BusyCounter<int> busyFlag(initList);
	while (index <= otherIndex)
	{
		ctrlList.SetItemText(index, 0, items[index].name.c_str());
		ctrlList.SetCheckState(index, items[index].enabled);
		++index;
	}
	ctrlList.SelectItem(selIndex);
	updateButtons(TRUE);
}

bool EmoticonPacksDlg::loadAvailablePacks()
{
	availPacks.clear();
	for (FileFindIter it(Text::utf8ToWide(Util::getEmoPacksPath() + "*.xml")); it != FileFindIter::end; ++it)
	{
		wstring name = it->getFileNameW();
		auto pos = name.rfind(L'.');
		name.erase(pos);
		availPacks.push_back(name);
	}
	return !availPacks.empty();
}

#endif // BL_UI_FEATURE_EMOTICONS

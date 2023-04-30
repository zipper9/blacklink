#include "stdafx.h"
#include "../client/SettingsManager.h"
#include "SearchUrlsPage.h"
#include "DialogLayout.h"
#include "ImageLists.h"
#include "WinUtil.h"
#include <boost/algorithm/string/trim.hpp>

using DialogLayout::FLAG_TRANSLATE;
using DialogLayout::UNSPEC;
using DialogLayout::AUTO;

static const DialogLayout::Item layoutItems1[] =
{
	{ IDC_ADD_MENU,    FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_CHANGE_MENU, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_REMOVE_MENU, FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_MOVE_UP,     FLAG_TRANSLATE, UNSPEC, UNSPEC },
	{ IDC_MOVE_DOWN,   FLAG_TRANSLATE, UNSPEC, UNSPEC }
};

static const DialogLayout::Align align1 = { 3, DialogLayout::SIDE_RIGHT, U_DU(6) };

static const DialogLayout::Item layoutItems2[] =
{
	{ IDC_CAPTION_DESCRIPTION, FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_CAPTION_URL,         FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDC_CAPTION_TYPE,        FLAG_TRANSLATE, AUTO,   UNSPEC             },
	{ IDC_TYPE,                0,              UNSPEC, UNSPEC, 0, &align1 },
	{ IDOK,                    FLAG_TRANSLATE, UNSPEC, UNSPEC             },
	{ IDCANCEL,                FLAG_TRANSLATE, UNSPEC, UNSPEC             }
};

static const tstring& printType(SearchUrl::Type type)
{
	switch (type)
	{
		case SearchUrl::KEYWORD:
			return TSTRING(SEARCH_URL_KEYWORD);
		case SearchUrl::HOSTNAME:
			return TSTRING(SEARCH_URL_HOSTNAME);
		case SearchUrl::IP4:
			return TSTRING(IPV4);
		case SearchUrl::IP6:
			return TSTRING(IPV6);
	}
	return Util::emptyStringT;
}

class SearchUrlDlg : public CDialogImpl<SearchUrlDlg>
{
	public:
		enum { IDD = IDD_SEARCH_URL };

		BEGIN_MSG_MAP(SearchUrlDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_HANDLER(IDC_DESCRIPTION, EN_CHANGE, onChange)
		COMMAND_HANDLER(IDC_URL, EN_CHANGE, onChange)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	public:
		tstring description;
		tstring url;
		SearchUrl::Type type = SearchUrl::KEYWORD;
};

LRESULT SearchUrlDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(SETTINGS_SEARCH_URL));
	DialogLayout::layout(m_hWnd, layoutItems2, _countof(layoutItems2));

	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::WEB_SEARCH, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);

	boost::trim(description);
	GetDlgItem(IDC_DESCRIPTION).SetWindowText(description.c_str());
	boost::trim(url);
	GetDlgItem(IDC_URL).SetWindowText(url.c_str());

	CComboBox ctrlType(GetDlgItem(IDC_TYPE));
	for (int i = 0; i <= SearchUrl::MAX_TYPE; i++)
		ctrlType.AddString(printType((SearchUrl::Type) i).c_str());
	ctrlType.SetCurSel((int) type);

	if (description.empty() || url.empty())
		GetDlgItem(IDOK).EnableWindow(FALSE);

	CenterWindow(GetParent());
	return 0;
}

LRESULT SearchUrlDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::getWindowText(GetDlgItem(IDC_DESCRIPTION), description);
		boost::trim(description);
		WinUtil::getWindowText(GetDlgItem(IDC_URL), url);
		boost::trim(url);
		if (description.empty() || url.empty()) return 0;
		type = (SearchUrl::Type) CComboBox(GetDlgItem(IDC_TYPE)).GetCurSel();
	}
	EndDialog(wID);
	return 0;
}

LRESULT SearchUrlDlg::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CButton ctrlOk(GetDlgItem(IDOK));
	tstring s;
	WinUtil::getWindowText(GetDlgItem(IDC_DESCRIPTION), s);
	boost::trim(s);
	if (s.empty())
	{
		ctrlOk.EnableWindow(FALSE);
		return 0;
	}
	WinUtil::getWindowText(GetDlgItem(IDC_URL), s);
	boost::trim(s);
	if (s.empty())
	{
		ctrlOk.EnableWindow(FALSE);
		return 0;
	}
	ctrlOk.EnableWindow(TRUE);
	return 0;
}

LRESULT SearchUrlsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	DialogLayout::layout(m_hWnd, layoutItems1, _countof(layoutItems1));

	CRect rc;
	ctrlList.Attach(GetDlgItem(IDC_MENU_ITEMS));
	ctrlList.GetClientRect(rc);

	ctrlList.InsertColumn(0, CTSTRING(DESCRIPTION), LVCFMT_LEFT, rc.Width() * 2 / 6, 0);
	ctrlList.InsertColumn(1, CTSTRING(WEB_URL), LVCFMT_LEFT, rc.Width() * 3 / 6, 1);
	ctrlList.InsertColumn(2, CTSTRING(TYPE), LVCFMT_LEFT, rc.Width() / 6, 2);

	ctrlList.SetExtendedListViewStyle(WinUtil::getListViewExStyle(false));
	WinUtil::setExplorerTheme(ctrlList);

	insertUrlList();
	return 0;
}

void SearchUrlsPage::insertUrlList()
{
	const auto& lst = FavoriteManager::getInstance()->getSearchUrls();
	int cnt = ctrlList.GetItemCount();
	for (const SearchUrl& url : lst)
	{
		data.push_back(url);
		addEntry(url, cnt++);
	}
	updateButtons();
}

void SearchUrlsPage::addEntry(const SearchUrl& url, int pos)
{
	TStringList lst;
	lst.push_back(Text::toT(url.description));
	lst.push_back(Text::toT(url.url));
	lst.push_back(printType(url.type));
	ctrlList.insert(pos, lst, 0, 0);
}

void SearchUrlsPage::setEntry(const SearchUrl& url, int pos)
{
	ctrlList.SetItemText(pos, 0, Text::toT(url.description).c_str());
	ctrlList.SetItemText(pos, 1, Text::toT(url.url).c_str());
	ctrlList.SetItemText(pos, 2, printType(url.type).c_str());
}

void SearchUrlsPage::updateButtons()
{
	BOOL enable = (ctrlList.GetItemCount() > 0 && ctrlList.GetSelectedCount() == 1) ? TRUE : FALSE;
	GetDlgItem(IDC_CHANGE_MENU).EnableWindow(enable);
	GetDlgItem(IDC_REMOVE_MENU).EnableWindow(enable);
	int selIndex = enable ? ctrlList.GetSelectedIndex() : -1;
	GetDlgItem(IDC_MOVE_UP).EnableWindow(enable && selIndex > 0);
	GetDlgItem(IDC_MOVE_DOWN).EnableWindow(enable && selIndex < (int) data.size() - 1);
}

LRESULT SearchUrlsPage::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SearchUrlDlg dlg;
	if (dlg.DoModal() == IDOK)
	{
		SearchUrl url;
		url.description = Text::fromT(dlg.description);
		url.url = Text::fromT(dlg.url);
		url.type = dlg.type;
		addEntry(url, ctrlList.GetItemCount());
		data.push_back(url);
		FavoriteManager::getInstance()->setSearchUrls(data);
	}
	updateButtons();
	return 0;
}

LRESULT SearchUrlsPage::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	updateButtons();
	return 0;
}

LRESULT SearchUrlsPage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD_MENU, 0);
			break;
/*
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE_MENU, 0);
			break;
*/
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT SearchUrlsPage::onChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlList.GetSelectedCount() == 1)
	{
		int sel = ctrlList.GetSelectedIndex();
		SearchUrl& url = data[sel];
		SearchUrlDlg dlg;
		dlg.description = Text::toT(url.description);
		dlg.url = Text::toT(url.url);
		dlg.type = url.type;
		if (dlg.DoModal() == IDOK)
		{
			url.description = Text::fromT(dlg.description);
			url.url = Text::fromT(dlg.url);
			url.type = dlg.type;
			FavoriteManager::getInstance()->setSearchUrls(data);
			setEntry(url, sel);
		}
	}

	return 0;
}

LRESULT SearchUrlsPage::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlList.GetSelectedCount() == 1)
	{
		int sel = ctrlList.GetSelectedIndex();
		data.erase(data.begin() + sel);
		FavoriteManager::getInstance()->setSearchUrls(data);
		ctrlList.DeleteItem(sel);
	}
	updateButtons();
	return 0;
}

void SearchUrlsPage::swapItems(int delta)
{
	if (ctrlList.GetSelectedCount() != 1) return;
	int index = ctrlList.GetSelectedIndex();
	int other = index + delta;
	if (other < 0 || other >= (int) data.size()) return;
	std::swap(data[index], data[other]);
	setEntry(data[index], index);
	setEntry(data[other], other);
	FavoriteManager::getInstance()->setSearchUrls(data);
	ctrlList.SelectItem(other);
}

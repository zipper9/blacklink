#include "stdafx.h"
#include "BaseHandlers.h"
#include "ImageLists.h"
#include "WinUtil.h"

void PreviewBaseHandler::appendPreviewItems(OMenu& menu)
{
	dcassert(_debugIsClean);
	dcdrun(_debugIsClean = false;)

	menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU) previewMenu, CTSTRING(PREVIEW_MENU), g_iconBitmaps.getBitmap(IconBitmaps::PREVIEW, 0));
}

void PreviewBaseHandler::activatePreviewItems(OMenu& menu)
{
	dcassert(!_debugIsActivated);
	dcdrun(_debugIsActivated = true;)
			
	int count = menu.GetMenuItemCount();
	MENUITEMINFO mii = { sizeof(mii) };
	// Passing HMENU to EnableMenuItem doesn't work with owner-draw OMenus for some reason
	mii.fMask = MIIM_SUBMENU;
	for (int i = 0; i < count; ++i)
	if (menu.GetMenuItemInfo(i, TRUE, &mii) && mii.hSubMenu == (HMENU) previewMenu)
	{
		menu.EnableMenuItem(i, MF_BYPOSITION | (previewMenu.GetMenuItemCount() > 0 ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
		break;
	}
}

void InternetSearchBaseHandler::appendInternetSearchItems(OMenu& menu)
{
	CMenu subMenu;
	subMenu.CreateMenu();
	subMenu.AppendMenu(MF_STRING, IDC_SEARCH_FILE_IN_GOOGLE, CTSTRING(SEARCH_WITH_GOOGLE));
	subMenu.AppendMenu(MF_STRING, IDC_SEARCH_FILE_IN_YANDEX, CTSTRING(SEARCH_WITH_YANDEX));
	menu.AppendMenu(MF_STRING, subMenu, CTSTRING(SEARCH_FILE_ON_INTERNET), g_iconBitmaps.getBitmap(IconBitmaps::INTERNET, 0));
	subMenu.Detach();
}

void InternetSearchBaseHandler::searchFileOnInternet(const WORD wID, const tstring& file)
{
	tstring url;
	switch (wID)
	{
		case IDC_SEARCH_FILE_IN_GOOGLE:
			url += _T("https://www.google.com/search?hl=") + Text::toT(Util::getLang()) + _T("&q=");
			break;
		case IDC_SEARCH_FILE_IN_YANDEX:
			url += _T("https://yandex.ru/yandsearch?text=");
			break;
		default:
			return;
	}
	url += file;
	WinUtil::openFile(url);
}

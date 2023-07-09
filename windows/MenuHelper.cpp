#include "stdafx.h"
#include "MenuHelper.h"
#include "PreviewMenu.h"
#include "resource.h"
#include "../client/QueueItem.h"

static boost::unordered_set<HMENU> staticMenus;

CMenu MenuHelper::mainMenu;
OMenu MenuHelper::copyHubMenu;

void MenuHelper::createMenus()
{
	mainMenu.CreateMenu();

	CMenuHandle file;
	file.CreatePopupMenu();

	file.AppendMenu(MF_STRING, IDC_OPEN_FILE_LIST, CTSTRING(MENU_OPEN_FILE_LIST));
	file.AppendMenu(MF_STRING, IDC_ADD_MAGNET, CTSTRING(MENU_ADD_MAGNET));
	file.AppendMenu(MF_STRING, IDC_OPEN_MY_LIST, CTSTRING(MENU_OPEN_OWN_LIST));
	file.AppendMenu(MF_STRING, IDC_REFRESH_FILE_LIST, CTSTRING(MENU_REFRESH_FILE_LIST));
	file.AppendMenu(MF_STRING, IDC_OPEN_DOWNLOADS, CTSTRING(MENU_OPEN_DOWNLOADS_DIR));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, IDC_OPEN_LOGS, CTSTRING(MENU_OPEN_LOGS_DIR));
	file.AppendMenu(MF_STRING, IDC_OPEN_CONFIGS, CTSTRING(MENU_OPEN_CONFIG_DIR));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_FILE_SETTINGS, CTSTRING(MENU_SETTINGS));
	file.AppendMenu(MF_SEPARATOR);
	file.AppendMenu(MF_STRING, ID_APP_EXIT, CTSTRING(MENU_EXIT));

	mainMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)file, CTSTRING(MENU_FILE));

	CMenuHandle view;
	view.CreatePopupMenu();
	view.AppendMenu(MF_STRING, IDC_PUBLIC_HUBS, CTSTRING(MENU_PUBLIC_HUBS));
	view.AppendMenu(MF_STRING, IDC_RECENTS, CTSTRING(MENU_FILE_RECENT_HUBS));
	view.AppendMenu(MF_STRING, IDC_FAVORITES, CTSTRING(MENU_FAVORITE_HUBS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, IDC_FAVUSERS, CTSTRING(MENU_FAVORITE_USERS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, ID_FILE_SEARCH, CTSTRING(MENU_SEARCH));
	view.AppendMenu(MF_STRING, IDC_FILE_ADL_SEARCH, CTSTRING(MENU_ADL_SEARCH));
	view.AppendMenu(MF_STRING, IDC_SEARCH_SPY, CTSTRING(MENU_SEARCH_SPY));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, IDC_DHT, CTSTRING(DHT_TITLE));
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
	view.AppendMenu(MF_STRING, IDC_CDMDEBUG_WINDOW, CTSTRING(MENU_CDMDEBUG_MESSAGES));
#endif
	view.AppendMenu(MF_STRING, IDC_NOTEPAD, CTSTRING(MENU_NOTEPAD));
	view.AppendMenu(MF_STRING, IDC_HASH_PROGRESS, CTSTRING(MENU_HASH_PROGRESS));
	view.AppendMenu(MF_SEPARATOR);
	view.AppendMenu(MF_STRING, IDC_TOPMOST, CTSTRING(MENU_TOPMOST));
	view.AppendMenu(MF_STRING, ID_VIEW_TOOLBAR, CTSTRING(MENU_TOOLBAR));
	view.AppendMenu(MF_STRING, ID_VIEW_STATUS_BAR, CTSTRING(MENU_STATUS_BAR));
	view.AppendMenu(MF_STRING, ID_VIEW_TRANSFER_VIEW, CTSTRING(MENU_TRANSFER_VIEW));
	view.AppendMenu(MF_STRING, ID_VIEW_MEDIA_TOOLBAR, CTSTRING(TOGGLE_TOOLBAR));
	view.AppendMenu(MF_STRING, ID_VIEW_QUICK_SEARCH, CTSTRING(TOGGLE_QSEARCH));

	mainMenu.AppendMenu(MF_POPUP, (UINT_PTR) (HMENU) view, CTSTRING(MENU_VIEW));

	CMenuHandle transfers;
	transfers.CreatePopupMenu();

	transfers.AppendMenu(MF_STRING, IDC_QUEUE, CTSTRING(MENU_DOWNLOAD_QUEUE));
	transfers.AppendMenu(MF_STRING, IDC_FINISHED, CTSTRING(MENU_FINISHED_DOWNLOADS));
	transfers.AppendMenu(MF_SEPARATOR);
	transfers.AppendMenu(MF_STRING, IDC_UPLOAD_QUEUE, CTSTRING(MENU_WAITING_USERS));
	transfers.AppendMenu(MF_STRING, IDC_FINISHED_UL, CTSTRING(MENU_FINISHED_UPLOADS));
	transfers.AppendMenu(MF_SEPARATOR);
	transfers.AppendMenu(MF_STRING, IDC_NET_STATS, CTSTRING(MENU_NETWORK_STATISTICS));

	mainMenu.AppendMenu(MF_POPUP, (UINT_PTR) (HMENU) transfers, CTSTRING(MENU_TRANSFERS));

	CMenuHandle tools;
	tools.CreatePopupMenu();

	tools.AppendMenu(MF_STRING, IDC_RECONNECT, CTSTRING(MENU_RECONNECT));
	tools.AppendMenu(MF_STRING, IDC_RECONNECT_DISCONNECTED, CTSTRING(MENU_RECONNECT_DISCONNECTED));
	tools.AppendMenu(MF_STRING, IDC_FOLLOW, CTSTRING(MENU_FOLLOW_REDIRECT));
	tools.AppendMenu(MF_STRING, IDC_QUICK_CONNECT, CTSTRING(MENU_QUICK_CONNECT));
	tools.AppendMenu(MF_SEPARATOR);
	tools.AppendMenu(MF_STRING, IDC_MATCH_ALL, CTSTRING(MENU_OPEN_MATCH_ALL));
	tools.AppendMenu(MF_STRING, ID_GET_TTH, CTSTRING(MENU_TTH));
	tools.AppendMenu(MF_STRING, IDC_DCLST_FROM_FOLDER, CTSTRING(MENU_DCLST_FROM_FOLDER));
	tools.AppendMenu(MF_SEPARATOR);
	tools.AppendMenu(MF_STRING, IDC_SHUTDOWN, CTSTRING(MENU_SHUTDOWN));

	mainMenu.AppendMenu(MF_POPUP, (UINT_PTR) (HMENU) tools, CTSTRING(MENU_TOOLS));

	CMenuHandle window;
	window.CreatePopupMenu();

	window.AppendMenu(MF_STRING, ID_WINDOW_CASCADE, CTSTRING(MENU_CASCADE));
	window.AppendMenu(MF_STRING, ID_WINDOW_TILE_HORZ, CTSTRING(MENU_HORIZONTAL_TILE));
	window.AppendMenu(MF_STRING, ID_WINDOW_TILE_VERT, CTSTRING(MENU_VERTICAL_TILE));
	window.AppendMenu(MF_STRING, ID_WINDOW_ARRANGE, CTSTRING(MENU_ARRANGE));
	window.AppendMenu(MF_STRING, ID_WINDOW_MINIMIZE_ALL, CTSTRING(MENU_MINIMIZE_ALL));
	window.AppendMenu(MF_STRING, ID_WINDOW_RESTORE_ALL, CTSTRING(MENU_RESTORE_ALL));
	window.AppendMenu(MF_SEPARATOR);
	window.AppendMenu(MF_STRING, IDC_CLOSE_DISCONNECTED, CTSTRING(MENU_CLOSE_DISCONNECTED));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_HUBS, CTSTRING(MENU_CLOSE_ALL_HUBS));
	window.AppendMenu(MF_STRING, IDC_CLOSE_HUBS_BELOW, CTSTRING(MENU_CLOSE_HUBS_BELOW));
	window.AppendMenu(MF_STRING, IDC_CLOSE_HUBS_NO_USR, CTSTRING(MENU_CLOSE_HUBS_EMPTY));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_PM, CTSTRING(MENU_CLOSE_ALL_PM));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_PM, CTSTRING(MENU_CLOSE_ALL_OFFLINE_PM));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_DIR_LIST, CTSTRING(MENU_CLOSE_ALL_DIR_LIST));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_DIR_LIST, CTSTRING(MENU_CLOSE_ALL_OFFLINE_DIR_LIST));
	window.AppendMenu(MF_STRING, IDC_CLOSE_ALL_SEARCH_FRAME, CTSTRING(MENU_CLOSE_ALL_SEARCHFRAME));

	mainMenu.AppendMenu(MF_POPUP, (UINT_PTR) (HMENU) window, CTSTRING(MENU_WINDOW));

	CMenuHandle help;
	help.CreatePopupMenu();

	help.AppendMenu(MF_STRING, ID_APP_ABOUT, CTSTRING(MENU_ABOUT));

	mainMenu.AppendMenu(MF_POPUP, (UINT_PTR) (HMENU) help, CTSTRING(MENU_HLP));

	copyHubMenu.CreatePopupMenu();
	copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBNAME, CTSTRING(HUB_NAME));
	copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBADDRESS, CTSTRING(HUB_ADDRESS));
	copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUB_IP, CTSTRING(IP_ADDRESS));

	staticMenus.insert((HMENU) copyHubMenu);
}

void MenuHelper::uninit()
{
	mainMenu.DestroyMenu();
	copyHubMenu.DestroyMenu();
	staticMenus.clear();
}

void MenuHelper::appendPrioItems(OMenu& menu, int idFirst)
{
	static const ResourceManager::Strings names[] =
	{
		ResourceManager::PAUSED,
		ResourceManager::LOWEST,
		ResourceManager::LOWER,
		ResourceManager::LOW,
		ResourceManager::NORMAL,
		ResourceManager::HIGH,
		ResourceManager::HIGHER,
		ResourceManager::HIGHEST
	};
	static_assert(_countof(names) == QueueItem::LAST, "priority list mismatch");
	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID;
	mii.fType = MFT_RADIOCHECK;
	int index = menu.GetMenuItemCount();
	for (int i = 0; i < _countof(names); i++)
	{
		mii.wID = idFirst + i;
		mii.dwTypeData = const_cast<TCHAR*>(CTSTRING_I(names[i]));
		menu.InsertMenuItem(index + i, TRUE, &mii);
	}
}

void MenuHelper::addStaticMenu(HMENU hMenu)
{
	dcassert(IsMenu(hMenu));
	staticMenus.insert(hMenu);
}

void MenuHelper::removeStaticMenu(HMENU hMenu)
{
	staticMenus.erase(hMenu);
}

void MenuHelper::unlinkStaticMenus(OMenu& menu)
{
	MENUITEMINFO mif = { sizeof MENUITEMINFO };
	mif.fMask = MIIM_SUBMENU;
	for (int i = menu.GetMenuItemCount()-1; i >= 0; i--)
	{
		ATLVERIFY(menu.GetMenuItemInfo(i, TRUE, &mif));
		if (!mif.hSubMenu) continue;
		if (PreviewMenu::isPreviewMenu(mif.hSubMenu) ||
		    staticMenus.contains(mif.hSubMenu))
		{
			menu.RemoveMenu(i, MF_BYPOSITION);
		}
	}
}

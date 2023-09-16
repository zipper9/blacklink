#include "stdafx.h"
#include "UCHandler.h"
#include "WinUtil.h"
#include "../client/FavoriteManager.h"

void UCHandlerBase::appendUcMenu(OMenu& menu, int ctx, const StringList& hubs)
{
	FavoriteManager::getInstance()->getUserCommands(userCommands, ctx, hubs);

	const bool isMe = (ctx & UserCommand::CONTEXT_FLAG_ME) != 0;
	const bool isTransfers = (ctx & UserCommand::CONTEXT_FLAG_TRANSFERS) != 0;
	const bool isMultiple = (ctx & UserCommand::CONTEXT_FLAG_MULTIPLE) != 0;
	ctx &= UserCommand::CONTEXT_MASK;

	const bool useSubMenu = BOOLSETTING(UC_SUBMENU) || isTransfers;
	const int prevCount = menu.GetMenuItemCount();
	menuPos = prevCount;

	bool addOpCommands = ctx != UserCommand::CONTEXT_HUB && ctx != UserCommand::CONTEXT_SEARCH && ctx != UserCommand::CONTEXT_FILELIST;
	if (addOpCommands)
	{
		if (!serviceSubMenu.m_hMenu)
			serviceSubMenu.CreatePopupMenu();
		else
			serviceSubMenu.ClearMenu();
		if (isTransfers)
		{
			serviceSubMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(CLOSE_CONNECTION), g_iconBitmaps.getBitmap(IconBitmaps::DISCONNECT, 0));
			serviceSubMenu.AppendMenu(MF_STRING, IDC_ADD_P2P_GUARD, CTSTRING(CLOSE_CONNECTION_AND_BLOCK_IP), g_iconBitmaps.getBitmap(IconBitmaps::WALL, 0));
		}
		if (!isMe) serviceSubMenu.AppendMenu(MF_STRING, IDC_GET_USER_RESPONSES, CTSTRING(GET_USER_RESPONSES));
		if (!isMultiple) serviceSubMenu.AppendMenu(MF_STRING, IDC_REPORT, CTSTRING(DUMP_USER_INFO));
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		if (!isMe) serviceSubMenu.AppendMenu(MF_STRING, IDC_CHECKLIST, CTSTRING(CHECK_FILELIST));
#endif
		if (!isMe) serviceSubMenu.AppendMenu(MF_STRING, IDC_REMOVEALL, CTSTRING(REMOVE_FROM_ALL));
		WinUtil::appendSeparator(menu);
		menu.AppendMenu(MF_POPUP, serviceSubMenu, CTSTRING(SERVICE_COMMANDS), g_iconBitmaps.getBitmap(IconBitmaps::HAMMER, 0));
	}

	int subMenuIndex = -1;
	if (useSubMenu)
	{
		if (!subMenu.m_hMenu)
			subMenu.CreatePopupMenu();
		else
			subMenu.ClearMenu();
		if (!addOpCommands) WinUtil::appendSeparator(menu);
		subMenuIndex = menu.GetMenuItemCount();
		menu.AppendMenu(MF_POPUP, subMenu, CTSTRING(USER_COMMANDS), g_iconBitmaps.getBitmap(IconBitmaps::COMMANDS, 0));
	}

	CMenuHandle cur = useSubMenu ? subMenu.m_hMenu : menu.m_hMenu;
	OMenu* menus[] = { &menu, &subMenu, nullptr };

	constexpr size_t MAX_TEXT_LEN = 511;
	bool firstCommand = true;
	for (size_t n = 0; n < userCommands.size(); ++n)
	{
		UserCommand& uc = userCommands[n];
		if (uc.getType() == UserCommand::TYPE_SEPARATOR)
			appendSeparator(menus, cur);
		if (uc.isRaw() || uc.isChat())
		{
			tstring name;
			const StringList displayName = uc.getDisplayName();
			cur = useSubMenu ? subMenu.m_hMenu : menu.m_hMenu;
			for (size_t i = 0; i < displayName.size(); ++i)
			{
				Text::toT(displayName[i], name);
				if (name.length() > MAX_TEXT_LEN) name.erase(MAX_TEXT_LEN);
				if (i + 1 == displayName.size())
				{
					if (firstCommand && !useSubMenu && i == 0)
					{
						appendSeparator(menus, cur);
						firstCommand = false;
					}
					appendMenu(menus, cur, MF_STRING, IDC_USER_COMMAND + n, name);
				}
				else
				{
					bool found = false;
					TCHAR buf[MAX_TEXT_LEN + 1];
					// Let's see if we find an existing item...
					int count = cur.GetMenuItemCount();
					for (int k = 0; k < count; k++)
					{
						if (cur.GetMenuState(k, MF_BYPOSITION) & MF_POPUP)
						{
							cur.GetMenuString(k, buf, _countof(buf), MF_BYPOSITION);
							if (stricmp(buf, name) == 0)
							{
								found = true;
								cur = (HMENU) cur.GetSubMenu(k);
								break;
							}
						}
					}
					if (!found)
					{
						if (firstCommand && !useSubMenu && i == 0)
						{
							appendSeparator(menus, cur);
							firstCommand = false;
						}
						HMENU newSubMenu = CreatePopupMenu();
						appendMenu(menus, cur, MF_POPUP, (UINT_PTR) newSubMenu, name);
						cur = newSubMenu;
					}
				}
			}
		}
	}
	if (subMenuIndex != -1 && userCommands.empty())
		menu.EnableMenuItem(subMenuIndex, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
	insertedItems = menu.GetMenuItemCount() - prevCount;
}

void UCHandlerBase::cleanUcMenu(OMenu& menu)
{
	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_SUBMENU;
	while (insertedItems)
	{
		ATLVERIFY(menu.GetMenuItemInfo(menuPos, TRUE, &mii));
		if (mii.hSubMenu == subMenu || mii.hSubMenu == serviceSubMenu)
			menu.RemoveMenu(menuPos, MF_BYPOSITION);
		else
			menu.DeleteMenu(menuPos, MF_BYPOSITION);
		insertedItems--;
	}
}

void UCHandlerBase::appendMenu(OMenu** oldMenu, CMenuHandle& menu, UINT flags, UINT_PTR id, const tstring& text)
{
	int index = 0;
	while (oldMenu[index])
	{
		if ((HMENU) *oldMenu[index] == (HMENU) menu)
		{
			oldMenu[index]->AppendMenu(flags, id, text.c_str());
			return;
		}
		++index;
	}
	menu.AppendMenu(flags, id, text.c_str());
}

void UCHandlerBase::appendSeparator(OMenu** oldMenu, CMenuHandle& menu)
{
	int index = 0;
	while (oldMenu[index])
	{
		if ((HMENU) *oldMenu[index] == (HMENU) menu)
		{
			WinUtil::appendSeparator(*oldMenu[index]);
			return;
		}
		++index;
	}
	WinUtil::appendSeparator(menu);
}

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

#ifndef UC_HANDLER_H
#define UC_HANDLER_H

#include "../client/FavoriteManager.h"
#include "../client/ClientManager.h"
#include "OMenu.h"
#include "WinUtil.h"

template<class T>
class UCHandler
{
	public:
		UCHandler() : menuPos(0), insertedItems(0)
		{
		}
		
		typedef UCHandler<T> thisClass;
		BEGIN_MSG_MAP(thisClass)
		COMMAND_RANGE_HANDLER(IDC_USER_COMMAND, IDC_USER_COMMAND + userCommands.size(), onUserCommand)
		END_MSG_MAP()
		
		LRESULT onUserCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(wID >= IDC_USER_COMMAND);
			size_t n = (size_t)wID - IDC_USER_COMMAND;
			dcassert(n < userCommands.size());
			
			UserCommand& uc = userCommands[n];
			
			T* t = static_cast<T*>(this);
			
			t->runUserCommand(uc);
			return 0;
		}
		
		void appendUcMenu(OMenu& menu, int ctx, const string& hubUrl)
		{
			appendUcMenu(menu, ctx, StringList(1, hubUrl));
		}
		
		void appendUcMenu(OMenu& menu, int ctx, const StringList& hubs)
		{
			FavoriteManager::getInstance()->getUserCommands(userCommands, ctx, hubs);

			const bool useSubMenu = BOOLSETTING(UC_SUBMENU);
			const int prevCount = menu.GetMenuItemCount();
			menuPos = prevCount;
			
			bool addOpCommands = ctx != UserCommand::CONTEXT_HUB && ctx != UserCommand::CONTEXT_SEARCH && ctx != UserCommand::CONTEXT_FILELIST;
			if (addOpCommands)
			{
				for (int i = 0; i < prevCount; i++)
					if (menu.GetMenuItemID(i) == IDC_REPORT)
					{
						addOpCommands = false;
						break;
					}
				if (addOpCommands)
				{
					if (prevCount) menu.AppendMenu(MF_SEPARATOR);
					menu.AppendMenu(MF_STRING, IDC_GET_USER_RESPONSES, CTSTRING(GET_USER_RESPONSES));
					menu.AppendMenu(MF_STRING, IDC_REPORT, CTSTRING(DUMP_USER_INFO));
#ifdef IRAINMAN_INCLUDE_USER_CHECK
					menu.AppendMenu(MF_STRING, IDC_CHECKLIST, CTSTRING(CHECK_FILELIST));
#endif
				}
			}

			if (useSubMenu)
			{
				if (!subMenu.m_hMenu)
					subMenu.CreatePopupMenu();
				else
					subMenu.ClearMenu();
				subMenu.InsertSeparatorFirst(TSTRING(USER_COMMANDS));
				WinUtil::appendSeparator(menu);
				menu.AppendMenu(MF_POPUP, (HMENU) subMenu, CTSTRING(USER_COMMANDS));
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
				if (uc.getType() == UserCommand::TYPE_RAW || uc.getType() == UserCommand::TYPE_RAW_ONCE)
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
			insertedItems = menu.GetMenuItemCount() - prevCount;
		}

		void cleanUcMenu(OMenu& menu)
		{
			MENUITEMINFO mii = { sizeof(mii) };
			mii.fMask = MIIM_SUBMENU;
			while (insertedItems)
			{
				menu.GetMenuItemInfo(menuPos, TRUE, &mii);
				if (mii.hSubMenu == subMenu)
					menu.RemoveMenu(menuPos, MF_BYPOSITION);
				else
					menu.DeleteMenu(menuPos, MF_BYPOSITION);
				insertedItems--;
			}
		}

	private:
		vector<UserCommand> userCommands;
		OMenu subMenu;
		int menuPos;
		int insertedItems;

		void appendMenu(OMenu** oldMenu, CMenuHandle& menu, UINT flags, UINT_PTR id, const tstring& text)
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

		void appendSeparator(OMenu** oldMenu, CMenuHandle& menu)
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
};

#endif // !defined(UC_HANDLER_H)

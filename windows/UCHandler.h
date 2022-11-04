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

#include "OMenu.h"
#include "UserCommand.h"
#include "resource.h"

class UCHandlerBase
{
	public:
		UCHandlerBase() : menuPos(0), insertedItems(0)
		{
		}

		void appendUcMenu(OMenu& menu, int ctx, const StringList& hubs);
		void appendUcMenu(OMenu& menu, int ctx, const string& hubUrl)
		{
			appendUcMenu(menu, ctx, StringList(1, hubUrl));
		}
		void cleanUcMenu(OMenu& menu);
		OMenu& getServiceSubMenu() { return serviceSubMenu; }

	protected:
		vector<UserCommand> userCommands;

	private:
		OMenu subMenu;
		OMenu serviceSubMenu;
		int menuPos;
		int insertedItems;

		void appendMenu(OMenu** oldMenu, CMenuHandle& menu, UINT flags, UINT_PTR id, const tstring& text);
		void appendSeparator(OMenu** oldMenu, CMenuHandle& menu);
};

template<class T>
class UCHandler : public UCHandlerBase
{
	public:
		typedef UCHandler<T> thisClass;
		BEGIN_MSG_MAP(thisClass)
		COMMAND_RANGE_HANDLER(IDC_USER_COMMAND, IDC_USER_COMMAND + userCommands.size(), onUserCommand)
		END_MSG_MAP()
		
		LRESULT onUserCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(wID >= IDC_USER_COMMAND);
			unsigned n = wID - IDC_USER_COMMAND;
			if (n < userCommands.size())
			{
				UserCommand& uc = userCommands[n];
				T* t = static_cast<T*>(this);
				t->runUserCommand(uc);
			}
			return 0;
		}
};

#endif // !defined(UC_HANDLER_H)

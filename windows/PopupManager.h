/*
 * Copyright (C) 2003-2013 Pär Björklund, per.bjorklund@gmail.com
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

#ifndef POPUPMANAGER_H
#define POPUPMANAGER_H

#include "../client/Singleton.h"
#include "PopupWindow.h"
#include "WinUtil.h"

class PopupManager : public Singleton<PopupManager>
{
	public:
		PopupManager();

		~PopupManager()
		{
			//dcassert(popups.empty());
		}

		enum
		{
			TYPE_SYSTEM,
			TYPE_CUSTOM
		};

		void show(const tstring& message, const tstring& title, int icon, bool preview = false);
		void remove(HWND hWnd);
		void removeAll();
		void autoRemove(uint64_t tick);
		void setEnabled(bool flag) { enabled = flag; }
		const string& getDefaultTitleFont();

	private:
		typedef std::list<PopupWindow*> PopupList;
		PopupList popups;
		bool enabled;
		string titleFont;
		string textFont;
		LOGFONT lfTitle;
		LOGFONT lfText;
		int offset;
		string defaultTitleFont;
};

#endif

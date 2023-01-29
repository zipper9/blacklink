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

#ifndef DCPLUSPLUS_CLIENT_USER_COMMAND_H
#define DCPLUSPLUS_CLIENT_USER_COMMAND_H

#include "Flags.h"
#include "BaseUtil.h"
#include "noexcept.h"

class UserCommand : public Flags
{
	public:
		enum
		{
			TYPE_SEPARATOR,
			TYPE_RAW,
			TYPE_RAW_ONCE,
			TYPE_REMOVE, // not used
			TYPE_CHAT,
			TYPE_CHAT_ONCE,
			TYPE_CLEAR = 255
		};

		enum
		{
			CONTEXT_HUB = 0x01,
			CONTEXT_USER = 0x02,
			CONTEXT_SEARCH = 0x04,
			CONTEXT_FILELIST = 0x08,
			CONTEXT_MASK = CONTEXT_HUB | CONTEXT_USER | CONTEXT_SEARCH | CONTEXT_FILELIST,
			CONTEXT_FLAG_ME = 0x100,
			CONTEXT_FLAG_TRANSFERS = 0x200,
			CONTEXT_FLAG_MULTIPLE = 0x400
		};

		enum
		{
			FLAG_NOSAVE = 0x01,
			FLAG_FROM_ADC_HUB = 0x02
		};
		
		UserCommand() : id(0), type(0), ctx(0) { }
		UserCommand(int id, int type, int ctx, Flags::MaskType flags, const string& name, const string& command, const string& to, const string& hub) noexcept
			:
			Flags(flags), id(id), type(type), ctx(ctx), name(name), command(command), to(to), hub(hub)
		{
		}
		bool isRaw() const
		{
			return type == TYPE_RAW || type == TYPE_RAW_ONCE;
		}
		bool isChat() const
		{
			return type == TYPE_CHAT || type == TYPE_CHAT_ONCE;
		}
		bool once() const
		{
			return type == TYPE_RAW_ONCE || type == TYPE_CHAT_ONCE;
		}
		
		StringList getDisplayName() const;
		
		GETSET(int, id, Id);
		GETSET(int, type, Type);
		GETSET(int, ctx, Ctx);
		GETSET(string, name, Name);
		GETSET(string, command, Command);
		GETSET(string, to, To);
		GETSET(string, hub, Hub);
};

#endif // DCPLUSPLUS_CLIENT_USER_COMMAND_H

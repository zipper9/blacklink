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

#ifndef DCPLUSPLUS_DCPP_CHAT_MESSAGE_H
#define DCPLUSPLUS_DCPP_CHAT_MESSAGE_H

#include "forward.h"
#include "typedefs.h"

class Identity;

class ChatMessage
{
	public:
		string text;
		OnlineUserPtr from;
		OnlineUserPtr to;
		OnlineUserPtr replyTo;
		bool thirdPerson;

		ChatMessage(const string& text, const OnlineUserPtr& from, const OnlineUserPtr& to = nullptr, const OnlineUserPtr& replyTo = nullptr, bool thirdPerson = false)
			: text(text), from(from), to(to), replyTo(replyTo), thirdPerson(thirdPerson), timestamp(0)
		{
		}

		ChatMessage(const ChatMessage&) = delete;
		ChatMessage& operator= (const ChatMessage&) = delete;

		bool isPrivate() const { return to && replyTo; }
		static string formatNick(const string& nick, const bool thirdPerson)
		{
			// let's *not* obey the spec here and add a space after the star. :P
			return thirdPerson ? "* " + nick + ' ' : '<' + nick + "> ";
		}
		void translateMe();
		void setTimestamp(time_t ts) { timestamp = ts; }
		string format() const;
		static string getExtra(const Identity& id);
		void getUserParams(StringMap& params, const string& hubUrl, bool myMessage) const;

	private:
		time_t timestamp;
};

#endif // !defined(DCPLUSPLUS_DCPP_CHAT_MESSAGE_H)

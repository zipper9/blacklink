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

#include "stdinc.h"
#include "UserCommand.h"
#include "SimpleStringTokenizer.h"
#include "Util.h"

StringList UserCommand::getDisplayName() const
{
	StringList displayName;
	string name2 = name;
	if (!isSet(UserCommand::FLAG_NOSAVE))
	{
		std::replace(name2.begin(), name2.end(), '\\', '/');
	}
	Util::replace("//", "\t", name2);
	SimpleStringTokenizer<char> t(name2, '/');
	string token;
	while (t.getNextToken(token))
	{
		std::replace(token.begin(), token.end(), '\t', '/');
		displayName.push_back(token);
	}
	return displayName;
}

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

#ifndef URI_UTIL_H_
#define URI_UTIL_H_

#include "typedefs.h"

namespace Util
{
	struct ParsedUrl
	{
		string protocol;
		string host;
		string user;
		string password;
		uint16_t port;
		string path;
		string query;
		string fragment;
		bool isSecure;

		void clearUser() { user.clear(); password.clear(); }
		void clearPath() { path.clear(); query.clear(); fragment.clear(); }
	};

	void decodeUrl(const string& url, ParsedUrl& res, const string& defProto = "dchub");
	string formatUrl(const ParsedUrl& p, bool omitDefaultPort);
	bool getDefaultPort(const string& protocol, uint16_t& port, bool& secure);

	string encodeUriQuery(const string& str);
	string encodeUriPath(const string& str);
	string decodeUri(const string& str);

	bool parseIpPort(const string& ipPort, string& ip, uint16_t& port);

	std::map<string, string> decodeQuery(const string& query);
	string getQueryParam(const string& query, const string& key);
}

#endif // URI_UTIL_H_

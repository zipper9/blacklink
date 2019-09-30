/*
 * Copyright (C) 2001-2019 Jacek Sieka, arnetheduck on gmail point com
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

#include "HubEntry.h"
#include "HublistManager.h"
#include "BZUtils.h"
#include "FilteredFile.h"
#include "SimpleXML.h"
#include "StringTokenizer.h"

#define RLock CFlyLock
#define WLock CFlyLock

static const string strHTTP("http://");
static const string strHTTPS("https://");
static const string strBZ2(".bz2");

static inline bool isValidURL(const string &s)
{
	return Text::isAsciiPrefix2(s, strHTTP) || Text::isAsciiPrefix2(s, strHTTPS);
}

class XmlListLoader : public SimpleXMLReader::CallBack
{
	public:
		XmlListLoader(HubEntry::List &lst) : publicHubs(lst) {}
		~XmlListLoader() {}

		void startTag(const string &aName, StringPairList &attribs, bool)
		{
			if (aName == "Hub")
			{
				const string &name = getAttrib(attribs, "Name", 0);
				const string &server = getAttrib(attribs, "Address", 1);
				const string &description = getAttrib(attribs, "Description", 2);
				const string &users = getAttrib(attribs, "Users", 3);
				const string &country = getAttrib(attribs, "Country", 4);
				const string &shared = getAttrib(attribs, "Shared", 5);
				const string &minShare = getAttrib(attribs, "Minshare", 5);
				const string &minSlots = getAttrib(attribs, "Minslots", 5);
				const string &maxHubs = getAttrib(attribs, "Maxhubs", 5);
				const string &maxUsers = getAttrib(attribs, "Maxusers", 5);
				const string &reliability = getAttrib(attribs, "Reliability", 5);
				const string &rating = getAttrib(attribs, "Rating", 5);
				if (!name.empty() && !server.empty())
					publicHubs.push_back(HubEntry(name, server, description, users, country, shared, minShare,
						minSlots, maxHubs, maxUsers, reliability, rating));
			}
		}
	
		void endTag(const string& name, const string& data) {}

	private:
		HubEntry::List &publicHubs;
};

HublistManager::HubList::HubList(uint64_t id, const string &url) noexcept : id(id), url(url)
{
	state = STATE_IDLE;
	conn = nullptr;
	lastModified = 0;
	used = true;
}

void HublistManager::HubList::getInfo(HublistManager::HubListInfo &info) const noexcept
{
	info.id = id;
	info.url = url;
	info.lastRedirUrl = lastRedirUrl;
	info.list = list;
	info.state = state;
	info.error = error;
	info.lastModified = lastModified;
}

void HublistManager::HubList::parse(int listType) noexcept
{
	if (downloadBuf.empty())
	{
		state = STATE_PARSE_FAILED;
		return;
	}
	try
	{
		list.clear();
		XmlListLoader loader(list);
		MemoryInputStream mis(downloadBuf);

		if (listType == TYPE_BZIP2)
		{
			FilteredInputStream<UnBZFilter, false> f(&mis);
			SimpleXMLReader(&loader).parse(f);
		}
		else
		{
			SimpleXMLReader(&loader).parse(mis);
		}
		state = STATE_DOWNLOADED;
	}
	catch (const Exception &e)
	{
		dcdebug("HublistManager::HubList::parse: %s\n", e.getError().c_str());
		state = STATE_PARSE_FAILED;
		error = e.getError();
	}
}

void HublistManager::HubList::save() const noexcept
{
	try
	{
		string path = Util::getHubListsPath();
		File::ensureDirectory(path);
		File f(path + Util::validateFileName(url), File::WRITE, File::CREATE | File::TRUNCATE);
		f.write(downloadBuf);
	}
	catch (const FileException &)
	{
	}
}

void HublistManager::HubList::getListData(bool forceDownload, HublistManager *manager) noexcept
{
	if (!forceDownload)
	{
		string path = Util::getHubListsPath() + Util::validateFileName(url);
		if (File::getSize(path) > 0) // FIXME: this uses FileFindIter!
		{
			int listType = Util::checkFileExt(path, strBZ2) ? TYPE_BZIP2 : TYPE_NORMAL;
			try
			{
				File cached(path, File::READ, File::OPEN);
				downloadBuf = cached.read();
				lastModified = cached.getLastModified();
			}
			catch (const FileException &)
			{
				downloadBuf.clear();
			}
			if (!downloadBuf.empty())
			{
				parse(listType);
				if (state != STATE_PARSE_FAILED) state = STATE_FROM_CACHE;
				return;
			}
		}
	}
	if (!conn)
	{
		conn = new HttpConnection(id);
		conn->addListener(manager);
	}
	downloadBuf.clear();
	lastRedirUrl.clear();
	state = STATE_DOWNLOADING;
	//conn->abortRequest();
	conn->downloadFile(url);
}

HublistManager::HublistManager(): nextID(0)
{
}

HublistManager::~HublistManager()
{
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.conn)
		{
			hl.conn->removeListener(this);
			delete hl.conn;
		}
	}
}

void HublistManager::getHubLists(vector<HubListInfo> &result) const noexcept
{
	result.clear();
	HubListInfo hli;
	cs.lock();
	result.reserve(hubLists.size());
	for (auto i = hubLists.cbegin(); i != hubLists.cend(); ++i)
	{
		i->getInfo(hli);
		result.push_back(hli);
	}
	cs.unlock();
}

bool HublistManager::getHubList(HubListInfo &result, uint64_t id) const noexcept
{
	cs.lock();
	for (auto i = hubLists.cbegin(); i != hubLists.cend(); ++i)
		if (i->id == id)
		{
			i->getInfo(result);
			cs.unlock();
			return true;
		}
	cs.unlock();
	return false;
}

bool HublistManager::refreshAndGetHubList(HubListInfo &result, uint64_t id) noexcept
{
	cs.lock();
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.id == id)
		{
			if (hl.state == STATE_IDLE)
				hl.getListData(false, this);
			hl.getInfo(result);
			cs.unlock();
			return true;
		}
	}
	cs.unlock();
	return false;
}

bool HublistManager::refresh(uint64_t id) noexcept
{
	bool result = false;
	cs.lock();
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.id == id)
		{
			hl.getListData(true, this);
			cs.unlock();
			result = true;
			break;
		}
	}
	cs.unlock();
	if (!result) return false;
	fire(HublistManagerListener::StateChanged(), id);	
	return true;
}

void HublistManager::setServerList(const string &str)
{
	cs.lock();
	for (auto j = hubLists.begin(); j != hubLists.end(); ++j)
		j->used = false;

	StringTokenizer<string> tokenizer(str, ';');
	const auto &list = tokenizer.getTokens();
	for (auto i = list.cbegin(); i != list.cend(); ++i)
	{
		const string &server = *i;
		if (!isValidURL(server)) continue;
		bool found = false;
		for (auto j = hubLists.begin(); j != hubLists.end(); ++j)
		{
			HubList &hl = *j;
			if (hl.url == server)
			{
				hl.used = true;
				found = true;
				break;
			}
		}
		if (!found)
			hubLists.emplace_back(++nextID, server);
	}
	for (auto j = hubLists.cbegin(); j != hubLists.cend();)
	{
		const HubList &hl = *j;
		if (!hl.used) hubLists.erase(j++); else ++j;
	}
	cs.unlock();
}

// HttpConnectionListener
void HublistManager::on(Data, HttpConnection *conn, const uint8_t *buf, size_t len) noexcept
{
	auto id = conn->getID();
	cs.lock();
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.id == id)
		{
			if (hl.state == STATE_DOWNLOADING)
				hl.downloadBuf.append((const char *) buf, len);
			break;
		}
	}
	cs.unlock();
}

void HublistManager::on(Failed, HttpConnection *conn, const string &aLine) noexcept
{
	auto id = conn->getID();
	cs.lock();
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.id == id)
		{
			if (hl.state == STATE_DOWNLOADING)
			{
				hl.state = STATE_DOWNLOAD_FAILED;
				hl.error = aLine;
				hl.downloadBuf.clear();
			}
			break;
		}
	}
	cs.unlock();
	fire(HublistManagerListener::StateChanged(), id);
}

void HublistManager::on(Complete, HttpConnection *conn, const string &) noexcept
{
	int listType =
		(conn->getMimeType() == "application/x-bzip2" ||
		Util::checkFileExt(conn->getPath(), strBZ2)) ? TYPE_BZIP2 : TYPE_NORMAL;
	auto id = conn->getID();
	cs.lock();
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.id == id)
		{
			if (hl.state == STATE_DOWNLOADING)
			{
				hl.parse(listType);
				hl.save();
				hl.downloadBuf.clear();
			}
			break;
		}
	}
	cs.unlock();
	fire(HublistManagerListener::StateChanged(), id);
}

void HublistManager::on(Redirected, HttpConnection *conn, const string &location) noexcept
{
	auto id = conn->getID();
	cs.lock();
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.id == id)
		{
			if (hl.state == STATE_DOWNLOADING)
				hl.lastRedirUrl = location;
			break;
		}
	}
	cs.unlock();
	fire(HublistManagerListener::StateChanged(), id);
}

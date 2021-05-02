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
#include "SimpleStringTokenizer.h"
#include "TimerManager.h"

static const unsigned MAX_CACHED_AGE = 3600 * 24 * 2; // 2 days

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

		void startTag(const string &tagName, StringPairList &attribs, bool)
		{
			if (tagName == "Hub")
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
				const string &encoding = getAttrib(attribs, "Encoding", 5);
				const string &secureUrl = getAttrib(attribs, "Secure", 2);
				const string &website = getAttrib(attribs, "Website", 6);
				const string &email = getAttrib(attribs, "Email", 6);
				const string &software = getAttrib(attribs, "Software", 6);
				const string &network = getAttrib(attribs, "Network", 6);
				if (!name.empty() && !server.empty())
					publicHubs.emplace_back(name, server, description, users, country, shared, minShare,
						minSlots, maxHubs, maxUsers, reliability, rating,
						encoding, secureUrl, website, email,
						software, network);
			}
		}

		void endTag(const string& name, const string& data) {}

	private:
		HubEntry::List &publicHubs;
};

static string getHublistPath(const string& url)
{
	string path = Util::getHubListsPath();
#ifdef _WIN32
	path += Util::validateFileName(url);
#else
	string tmp = url;
	std::replace(tmp.begin(), tmp.end(), '/', '_');
	path += Util::validateFileName(tmp);
#endif
	return path;
}

HublistManager::HubList::HubList(uint64_t id, const string &url) noexcept : id(id), url(url)
{
	state = STATE_IDLE;
	flags = 0;
	conn = nullptr;
	lastModified = 0;
	doRedirect = false;
}

HublistManager::HubList::HubList(HubList &&src) noexcept : id(src.id), url(src.url)
{
	lastRedirUrl = std::move(src.lastRedirUrl);
	doRedirect = src.doRedirect;
	list = std::move(src.list);
	state = src.state;
	flags = src.flags;
	conn = src.conn;
	downloadBuf = std::move(src.downloadBuf);
	error = std::move(src.error);
	lastModified = src.lastModified;
	src.conn = nullptr;
}

void HublistManager::HubList::getInfo(HublistManager::HubListInfo &info) const noexcept
{
	info.id = id;
	info.url = url;
	info.lastRedirUrl = lastRedirUrl;
	info.doRedirect = doRedirect;
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
		string path = getHublistPath(url);
		File::ensureDirectory(path);
		File f(path, File::WRITE, File::CREATE | File::TRUNCATE);
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
		string path = getHublistPath(url);
		if (File::getSize(path) > 0)
		{
			int listType = Util::checkFileExt(path, strBZ2) ? TYPE_BZIP2 : TYPE_NORMAL;
			try
			{
				File cached(path, File::READ, File::OPEN);
				time_t cachedTime = cached.getLastModified();
				if (cachedTime + MAX_CACHED_AGE >= GET_TIME())
				{
					downloadBuf = cached.read();
					lastModified = cachedTime;
				}
				else
					downloadBuf.clear();
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
	conn->setMaxBodySize(1024*1024);
	conn->setUserAgent(getHttpUserAgent());
	conn->downloadFile(url);
}

bool HublistManager::HubList::processRediect() noexcept
{
	if (!conn || !doRedirect || lastRedirUrl.empty()) return false;
	doRedirect = false;
	downloadBuf.clear();
	state = STATE_DOWNLOADING;
	conn->downloadFile(lastRedirUrl);
	return true;
}

HublistManager::HublistManager(): nextID(0)
{
}

HublistManager::~HublistManager() noexcept
{
	shutdown();
}

void HublistManager::shutdown() noexcept
{
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.conn)
		{
			hl.conn->removeListeners();
			delete hl.conn;
		}
	}
	hubLists.clear();
}

void HublistManager::removeUnusedConnections() noexcept
{
	cs.lock();
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.state != STATE_DOWNLOADING && !hl.doRedirect && hl.conn)
		{
			hl.conn->removeListeners();
			delete hl.conn;
			hl.conn = nullptr;
		}
	}
	cs.unlock();
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
			result = true;
			break;
		}
	}
	cs.unlock();
	if (!result) return false;
	fire(HublistManagerListener::StateChanged(), id);	
	return true;
}

bool HublistManager::processRedirect(uint64_t id) noexcept
{
	bool result = false;
	cs.lock();
	for (auto i = hubLists.begin(); i != hubLists.end(); ++i)
	{
		HubList &hl = *i;
		if (hl.id == id)
		{
			result = hl.processRediect();
			break;
		}
	}
	cs.unlock();
	if (!result) return false;
	fire(HublistManagerListener::StateChanged(), id);
	return true;
}

void HublistManager::setServerList(const string &str) noexcept
{
	static const int FLAG_UNUSED = 1;
	std::list<HubList> newHubLists;
	cs.lock();
	for (HubList& hl : hubLists) hl.flags = FLAG_UNUSED;
	SimpleStringTokenizer<char> tokenizer(str, ';');
	string server;
	while (tokenizer.getNextNonEmptyToken(server))
	{
		if (!isValidURL(server)) continue;
		bool found = false;
		for (auto j = hubLists.begin(); j != hubLists.end(); ++j)
		{
			HubList &hl = *j;
			if (hl.url == server)
			{
				newHubLists.emplace_back(std::move(hl));
				hl.flags &= ~FLAG_UNUSED;
				found = true;
				break;
			}
		}
		if (!found)
			newHubLists.emplace_back(++nextID, server);
	}
	for (HubList& hl : hubLists)
		if ((hl.flags & FLAG_UNUSED) && hl.conn)
		{
			hl.conn->removeListeners();
			delete hl.conn;
		}
	hubLists = std::move(newHubLists);
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
			{
				hl.state = STATE_DOWNLOAD_FAILED;
				hl.lastRedirUrl = location;
				hl.doRedirect = true;
			}
			break;
		}
	}
	cs.unlock();
	fire(HublistManagerListener::Redirected(), id);
}

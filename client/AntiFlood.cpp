#include "stdinc.h"
#include "AntiFlood.h"
#include "StrUtil.h"
#include "Util.h"
#include "SettingsManager.h"
#include "ConfCore.h"

IpBans udpBans;
IpBans tcpBans;

IpBans::IpBans() : dataLock(RWLock::create())
{
}

int IpBans::checkBan(const IpPortKey& key, int64_t timestamp) const
{
	READ_LOCK(*dataLock);
	auto i = data.find(key);
	if (i == data.end()) return NO_BAN;
	const BanInfo& info = i->second;
	if (info.dontBan) return DONT_BAN;
	return timestamp > info.unbanTime ? BAN_EXPIRED : BAN_ACTIVE;
}

void IpBans::addBan(const IpPortKey& key, int64_t timestamp, const string& url, int64_t reqCount)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	unsigned banDuration = (unsigned) ss->getInt(Conf::ANTIFLOOD_BAN_TIME) * 1000;
	ss->unlockRead();

	WRITE_LOCK(*dataLock);
	BanInfo& info = data[key];
	info.dontBan = false;
	info.unbanTime = timestamp + banDuration;
	info.reqCount = reqCount;
	if (std::find(info.hubUrls.begin(), info.hubUrls.end(), url) == info.hubUrls.end())
		info.hubUrls.push_back(url);
}

void IpBans::removeBan(const IpPortKey& key)
{
	WRITE_LOCK(*dataLock);
	auto i = data.find(key);
	if (i == data.end()) return;
	if (!i->second.dontBan) data.erase(i);
}

void IpBans::protect(const IpPortKey& key, bool enable)
{
	WRITE_LOCK(*dataLock);
	if (enable)
	{
		BanInfo& info = data[key];
		info.hubUrls.clear();
		info.unbanTime = 0;
		info.reqCount = 0;
		info.dontBan = true;
		return;
	}
	auto i = data.find(key);
	if (i == data.end()) return;
	if (i->second.dontBan) data.erase(i);
}

void IpBans::removeExpired(int64_t timestamp)
{
	WRITE_LOCK(*dataLock);
	for (auto i = data.begin(); i != data.end();)
	{
		const BanInfo& info = i->second;
		if (!info.dontBan && timestamp > info.unbanTime)
			data.erase(i++);
		else
			i++;
	}
}

string IpBans::getInfo(const string& type, int64_t timestamp) const
{
	string s;
	IpAddress ip;
	READ_LOCK(*dataLock);
	for (auto i = data.cbegin(); i != data.cend(); ++i)
	{
		const BanInfo& info = i->second;
		s += type;
		s += ' ';
		i->first.getIP(ip);
		s += Util::printIpAddress(ip, true);
		s += ':';
		s += Util::toString(i->first.port);
		s += " reqs=" + Util::toString(info.reqCount);
		if (!info.dontBan)
		{
			if (timestamp < info.unbanTime)
				s += " expires=" + Util::toString(static_cast<int>((info.unbanTime - timestamp) / 1000));
			else
				s += " expired";
			s += " hubs=";
			s += Util::toString(info.hubUrls);
		}
		else
			s += " protected";
		s += '\n';
	}
	return s;
}

bool HubRequestCounters::addRequest(IpBans& bans, const IpAddress& ip, uint16_t port, int64_t timestamp, const string& url, bool& showMsg)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	int minReqCount = ss->getInt(Conf::ANTIFLOOD_MIN_REQ_COUNT);
	unsigned maxReqPerMinute = ss->getInt(Conf::ANTIFLOOD_MAX_REQ_PER_MIN);
	ss->unlockRead();

	showMsg = false;
	IpPortKey key;
	key.setIP(ip, port);
	int res = bans.checkBan(key, timestamp);
	auto& item = data[key];
	item.rq.add(timestamp);
	int64_t reqCount = item.rq.getReqCount();
	bool flood = reqCount >= minReqCount && item.rq.getAvgPerMinute() > maxReqPerMinute;
	if (flood && !item.banned && res != IpBans::DONT_BAN)
	{
		item.banned = true;
		bans.addBan(key, timestamp, url, reqCount);
		res = IpBans::BAN_ACTIVE;
		showMsg = true;
	}
	if (res == IpBans::BAN_EXPIRED)
	{
		if (flood)
		{
			if (!item.banned)
			{
				item.banned = true;
				showMsg = true;
			}
			bans.addBan(key, timestamp, url, reqCount);
			res = IpBans::BAN_ACTIVE;
		}
		else
			bans.removeBan(key);
	}
	return res != IpBans::BAN_ACTIVE;
}

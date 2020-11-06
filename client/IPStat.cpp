#include "stdinc.h"
#include "IPStat.h"

void UserStatItem::addNick(const string& nickHub)
{
	auto i = std::find(nickList.begin(), nickList.end(), nickHub);
	if (i != nickList.end())
	{
		auto j = i;
		if (++j != nickList.end())
		{
			nickList.erase(i);
			nickList.emplace_back(nickHub);
		}
		return;
	}
	nickList.emplace_back(nickHub);
	if (nickList.size() > MAX_NICK_LIST_SIZE) nickList.pop_front();
	flags |= FLAG_CHANGED;
}

void UserStatItem::addNick(const string& nick, const string& hub)
{
	addNick(nick + '\t' + hub);
}

void UserStatItem::setIP(const string& ip)
{
	if (lastIp != ip)
	{
		lastIp = ip;
		flags |= FLAG_CHANGED;
	}
}

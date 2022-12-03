#ifndef IP_STAT_H_
#define IP_STAT_H_

#include "typedefs.h"
#include <stdint.h>
#include <list>

struct IPStatItem
{
	static const uint8_t FLAG_LOADED  = 1;
	static const uint8_t FLAG_CHANGED = 2;

	uint64_t download = 0;
	uint64_t upload = 0;
	uint8_t flags = 0;
};

struct IPStatVecItem
{
	string ip;
	IPStatItem item;
};

struct IPStatMap
{
	boost::unordered_map<string, IPStatItem> data;
	uint64_t totalDownloaded = 0;
	uint64_t totalUploaded = 0;
};

struct UserStatItem
{
	static const size_t MAX_NICK_LIST_SIZE = 16;
	static const uint8_t FLAG_LOADED  = 1;
	static const uint8_t FLAG_CHANGED = 2;

	string lastIp;
	std::list<string> nickList;
	unsigned messageCount = 0;
	uint8_t flags = 0;

	void addNick(const string& nickHub);
	void addNick(const string& nick, const string& hub);
	void setIP(const string& ip);
};

#endif // IP_STAT_H_

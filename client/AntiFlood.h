#ifndef ANTI_FLOOD_H_
#define ANTI_FLOOD_H_

#include "IpKey.h"
#include "RequestCounter.h"
#include "RWLock.h"

class IpBans
{
	public:
		enum
		{
			NO_BAN,
			BAN_ACTIVE,
			BAN_EXPIRED,
			DONT_BAN
		};

		IpBans();
		int checkBan(const IpPortKey& key, int64_t timestamp) const;
		void addBan(const IpPortKey& key, int64_t timestamp, const string& url, int64_t reqCount);
		void removeBan(const IpPortKey& key);
		void removeExpired(int64_t timestamp);
		void protect(const IpPortKey& key, bool enable);
		string getInfo(const string& type, int64_t timestamp) const;
		void setBanDuration(unsigned value);
		void updateSettings();

	private:
		struct BanInfo
		{
			int64_t unbanTime;
			int64_t reqCount;
			vector<string> hubUrls;
			bool dontBan;
		};

		boost::unordered_map<IpPortKey, BanInfo> data;
		mutable std::unique_ptr<RWLock> dataLock;
		unsigned banDuration;
};

// This class lacks data lock since it's only accessed from the hub thread
class ClientRequestCounters
{
	public:
		bool addRequest(IpBans& bans, const IpAddress& ip, uint16_t port, int64_t timestamp, const string& url, bool& showMsg);
		void setConfig(int minReqCount, unsigned maxReqPerMinute);

	private:
		struct IpItem
		{
			RequestCounter<120> rq;
			bool banned;
		};

		boost::unordered_map<IpPortKey, IpItem> data;
		int minReqCount = -1;
		unsigned maxReqPerMinute = 0;
};

#endif // ANTI_FLOOD_H_

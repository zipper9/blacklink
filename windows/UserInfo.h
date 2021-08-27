#ifndef USERINFO_H
#define USERINFO_H

#include "../client/ClientManager.h"
#include "../client/TaskQueue.h"
#include "../client/LocationUtil.h"
#include "../client/UserInfoColumns.h"

enum Tasks
{
	ADD_STATUS_LINE,
	ADD_CHAT_LINE,
	REMOVE_USER,
	REMOVE_USERS,
	UPDATE_USER_JOIN,
	UPDATE_USER,
	UPDATE_USERS,
	PRIVATE_MESSAGE,
	GET_PASSWORD,
	DISCONNECTED,
	CONNECTED,
	CHEATING_USER,
	USER_REPORT,
	LOAD_IP_INFO
};

class OnlineUserTask : public Task
{
	public:
		explicit OnlineUserTask(const OnlineUserPtr& ou) : ou(ou) {}
		const OnlineUserPtr ou;
};

class OnlineUsersTask : public Task
{
	public:
		explicit OnlineUsersTask(const vector<OnlineUserPtr>& userList) : userList(userList) {}
		vector<OnlineUserPtr> userList;
};

class MessageTask : public Task
{
	public:
		explicit MessageTask(const ChatMessage* messagePtr) : messagePtr(messagePtr) {}
		virtual ~MessageTask() { delete messagePtr; }
		const ChatMessage* getMessage() const { return messagePtr; }
	
	private:
		const ChatMessage* const messagePtr;
};

class UserInfo : public UserInfoBase
{
	private:
		const OnlineUserPtr ou;
		IPInfo ipInfo;

	public:
		static const uint16_t ALL_MASK = 0xFFFF;
		uint16_t flags;
		uint8_t stateP2PGuard;
		uint8_t stateLocation;

		enum
		{
			STATE_INITIAL,
			STATE_IN_PROGRESS,
			STATE_DONE
		};
		
		explicit UserInfo(const OnlineUserPtr& ou) : ou(ou), flags(ALL_MASK), stateP2PGuard(STATE_INITIAL), stateLocation(STATE_INITIAL)
		{
		}

		static int compareItems(const UserInfo* a, const UserInfo* b, int col);
		tstring getText(int col) const;
		bool isOP() const
		{
			return getIdentity().isOp();
		}
		IpAddress getIp() const
		{
			return getIdentity().getConnectIP();
		}
		uint8_t getImageIndex() const;
		static int getStateImageIndex() { return 0; }
		void loadP2PGuard();
		const IPInfo& getIpInfo() const
		{
			return ipInfo;
		}
		void loadLocation();
		void clearLocation();
		string getNick() const
		{
			return ou->getIdentity().getNick();
		}
#ifdef IRAINMAN_USE_HIDDEN_USERS
		bool isHidden() const
		{
			return ou->getIdentity().isHidden();
		}
#endif
		const OnlineUserPtr& getOnlineUser() const
		{
			return ou;
		}
		const UserPtr& getUser() const
		{
			return ou->getUser();
		}
		Identity& getIdentityRW()
		{
			return ou->getIdentity();
		}
		const Identity& getIdentity() const
		{
			return ou->getIdentity();
		}
		tstring getHubs() const
		{
			const Identity& id = ou->getIdentity();
			unsigned countNormal = id.getHubsNormal();
			unsigned countReg = id.getHubsRegistered();
			unsigned countOp = id.getHubsOperator();
			unsigned countHubs = countNormal + countReg + countOp;	
			if (countHubs)
			{
				TCHAR buf[64];
				_sntprintf(buf, _countof(buf), _T("%u (%u/%u/%u)"), countHubs, countNormal, countReg, countOp);
				return buf;
			}
			return Util::emptyStringT;
		}
		static tstring formatSpeedLimit(const uint32_t limit);
		static string getSpeedLimitText(int limit);
		tstring getLimit() const;
		tstring getDownloadSpeed() const;

		typedef boost::unordered_map<OnlineUserPtr, UserInfo*, OnlineUser::Hash> OnlineUserMap;
};

#endif //USERINFO_H

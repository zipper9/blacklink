#ifndef USERINFO_H
#define USERINFO_H

#include "../client/ClientManager.h"
#include "../client/TaskQueue.h"

#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
#include "../client/UserInfoColumns.h"
#endif

enum Tasks
{
	ADD_STATUS_LINE,
	ADD_CHAT_LINE,
	REMOVE_USER,
	PRIVATE_MESSAGE,
	UPDATE_USER_JOIN,
	GET_PASSWORD,
	DISCONNECTED,
	CONNECTED,
	UPDATE_USER,
	UPDATE_COLUMN_MESSAGE,
	UPADTE_COLUMN_DESC,
	CHEATING_USER,
	USER_REPORT,
	ASYNC_LOAD_PG_AND_GEI_IP,
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	UPADTE_COLUMN_SHARE
#endif
};

class OnlineUserTask : public Task
{
	public:
		explicit OnlineUserTask(const OnlineUserPtr& ou) : ou(ou) {}
		const OnlineUserPtr ou;
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
		Util::CustomNetworkIndex location;

	public:
		static const unsigned short ALL_MASK = 0xFFFF;
		unsigned short flags;
		char ownerDraw;
		
		explicit UserInfo(const OnlineUserPtr& ou) : ou(ou), flags(ALL_MASK), ownerDraw(0)
		{
		}

		static int compareItems(const UserInfo* a, const UserInfo* b, int col);
		bool isUpdate(int sortCol)
		{
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
			return (ou->getIdentity().getChanges() & (1 << sortCol)) != 0;
#else
			return true;
#endif
		}
		tstring getText(int col) const;
		bool isOP() const
		{
			return getIdentity().isOp();
		}
		string getIpAsString() const
		{
			return getIdentity().getIpAsString();
		}
		boost::asio::ip::address_v4 getIp() const
		{
			return getIdentity().getIp();
		}
		uint8_t getImageIndex() const
		{
			return UserInfoBase::getImage(*ou);
		}
		void calcP2PGuard();
		const Util::CustomNetworkIndex& getLocation() const
		{
			return location;
		}
		void calcLocation();
		void setLocation(const Util::CustomNetworkIndex& location)
		{
			this->location = location;
		}
		const string& getNick() const
		{
			return ou->getIdentity().getNick();
		}
		const tstring& getNickT() const
		{
			return ou->getIdentity().getNickT();
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
			return ou->getIdentity().getHubs();
		}
		static tstring formatSpeedLimit(const uint32_t limit);
		tstring getLimit() const;
		tstring getDownloadSpeed() const;

		typedef boost::unordered_map<OnlineUserPtr, UserInfo*, OnlineUser::Hash> OnlineUserMapBase;

		class OnlineUserMap : public OnlineUserMapBase
		{
			public:
				OnlineUserMap() {}
				OnlineUserMap(const OnlineUserMap&) = delete;
				OnlineUserMap& operator= (const OnlineUserMap&) = delete;
				
				UserInfo* findUser(const OnlineUserPtr& user) const
				{
					if (user->isFirstFind())
					{
						dcassert(find(user) == end());
						return nullptr;
					}
					else
					{
						const auto i = find(user);
						return i == end() ? nullptr : i->second;
					}
				}
		};
};

#endif //USERINFO_H

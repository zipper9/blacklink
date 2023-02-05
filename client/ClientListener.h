#ifndef CLIENTLISTENER_H_
#define CLIENTLISTENER_H_

#include "SearchQueue.h"
#include "ChatMessage.h"

class ClientBase;
class Client;
class CID;
class AdcCommand;

class ClientListener
{
	public:
		virtual ~ClientListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Connecting;
		typedef X<1> Connected;
		typedef X<2> LoggedIn;
		typedef X<3> UserUpdated;
		typedef X<4> UserListUpdated;
		typedef X<5> UserRemoved;
		typedef X<6> UserListRemoved;
		typedef X<7> Redirect;
		typedef X<8> ClientFailed;
		typedef X<9> GetPassword;
		typedef X<10> HubUpdated;
		typedef X<11> Message;
		typedef X<12> StatusMessage;
		typedef X<13> SettingsLoaded;
		typedef X<14> HubFull;
		typedef X<15> NickError;
		typedef X<18> AdcSearch;
		typedef X<19> CheatMessage;
		typedef X<20> HubInfoMessage;
		typedef X<21> UserReport;

		enum NickErrorCode
		{
			NoError,
			BadPassword,
			Taken,
			Rejected
		};

		enum HubInfoCode
		{
			HubTopic,
			OperatorInfo
		};
		
		enum StatusFlags
		{
			FLAG_NORMAL = 0x00,
			FLAG_IS_SPAM = 0x01
		};
		
		virtual void on(Connecting, const Client*) noexcept { }
		virtual void on(Connected, const Client*) noexcept { }
		virtual void on(LoggedIn, const Client*) noexcept { }
		virtual void on(UserUpdated, const OnlineUserPtr&) noexcept { }
		virtual void on(UserListUpdated, const ClientBase*, const OnlineUserList&) noexcept { }
		virtual void on(UserRemoved, const ClientBase*, const OnlineUserPtr&) noexcept { }
		virtual void on(UserListRemoved, const ClientBase*, const OnlineUserList&) noexcept { }
		virtual void on(Redirect, const Client*, const string&) noexcept { }
		virtual void on(ClientFailed, const Client*, const string&) noexcept { }
		virtual void on(GetPassword, const Client*) noexcept { }
		virtual void on(HubUpdated, const Client*) noexcept { }
		virtual void on(Message, const Client*, std::unique_ptr<ChatMessage>&) noexcept { }
		virtual void on(StatusMessage, const Client*, const string&, int = FLAG_NORMAL) noexcept { }
		virtual void on(SettingsLoaded, const Client*) noexcept { }
		virtual void on(HubFull, const Client*) noexcept { }
		virtual void on(NickError, NickErrorCode) noexcept { }
		virtual void on(AdcSearch, const Client*, const AdcCommand&, const OnlineUserPtr&) noexcept { }
		virtual void on(CheatMessage, const string&) noexcept { }
		virtual void on(HubInfoMessage, HubInfoCode, const Client*, const string&) noexcept { }
		virtual void on(UserReport, const ClientBase*, const string&) noexcept { }
};

#endif /*CLIENTLISTENER_H_*/

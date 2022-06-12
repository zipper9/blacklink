#ifndef SOCKET_POOL_H_
#define SOCKET_POOL_H_

#include "Socket.h"
#include "Locks.h"
#include <memory>

class SocketPool
{
	public:
		SocketPool();
		~SocketPool();
		uint16_t addSocket(const string& userKey, int af, bool serverRole, bool useTLS, bool allowUntrusted, const string& expKP) noexcept;
		Socket* takeSocket(uint16_t port) noexcept;
		void removeSocket(const string& userKey) noexcept;
		bool getPortForUser(const string& userKey, uint16_t& port, int& af) const noexcept;
		void removeExpired(uint64_t tick) noexcept;

	private:
		struct SocketInfo
		{
			Socket* s;
			string userKey;
			uint16_t port;
			uint64_t expires;
			int af;
		};
		typedef std::shared_ptr<SocketInfo> SocketInfoPtr;

		boost::unordered_map<uint16_t, SocketInfoPtr> socketsByPort;
		boost::unordered_map<string, SocketInfoPtr> socketsByUser;
		mutable CriticalSection csData;
		uint64_t removeTime;
};

extern SocketPool socketPool;

#endif // SOCKET_POOL_H_

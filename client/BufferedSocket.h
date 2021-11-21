#ifndef BUFFERED_SOCKET_H_
#define BUFFERED_SOCKET_H_

#include "Socket.h"
#include "BufferedSocketListener.h"
#include "Thread.h"
#include "Ip4Address.h"

class UnZFilter;
class InputStream;

class BufferedSocket : private Thread
{
	public:
		enum Modes
		{
			MODE_LINE,
			MODE_ZPIPE,
			MODE_DATA
		};

		enum NatRoles
		{
			NAT_NONE,
			NAT_CLIENT,
			NAT_SERVER
		};
		
		static BufferedSocket* getBufferedSocket(char separator, BufferedSocketListener* listener)
		{
			return new BufferedSocket(separator, listener);
		}

		static void destroyBufferedSocket(BufferedSocket* sock)
		{
			dcassert(!sock || !sock->isRunning());
			delete sock;
		}

		static void getBindAddress(IpAddress& ip, int af, const string& s);
		static void getBindAddress(IpAddress& ip, int af);

#ifdef FLYLINKDC_USE_SOCKET_COUNTER
		static void waitShutdown();
#endif

		void addAcceptedSocket(unique_ptr<Socket> newSock, uint16_t port);
		void connect(const string& address, uint16_t port, bool secure, 
			bool allowUntrusted, bool useProxy, Socket::Protocol proto, const string& expKP = Util::emptyString);
		void connect(const string& address, uint16_t port, uint16_t localPort, NatRoles natRole, bool secure, 
			bool allowUntrusted, bool useProxy, Socket::Protocol proto, const string& expKP = Util::emptyString);
		void disconnect(bool graceless);
		void updated();
		bool hasSocket() const
		{
			return sock.get() != nullptr;
		}
		void setMode(Modes newMode) noexcept;
		void setDataMode(int64_t bytes = -1)
		{
			mode = MODE_DATA;
			remainingSize = bytes;
		}
		bool isSecure() const
		{
			return hasSocket() && sock->getSecureTransport() == Socket::SECURE_TRANSPORT_SSL;
		}
		bool isTrusted() const
		{
			return hasSocket() && sock->isTrusted();
		}
		string getCipherName() const
		{
			return hasSocket() ? sock->getCipherName() : Util::emptyString;
		}
		string getRemoteIpAsString(bool brackets = false) const;
		string getRemoteIpPort() const;
		const IpAddress& getIp() const;
		const uint16_t getPort() const
		{
			return hasSocket() ? sock->getPort() : 0;
		}
		vector<uint8_t> getKeyprint() const
		{
			return sock->getKeyprint();
		}
		bool verifyKeyprint(const string& expKP, bool allowUntrusted) noexcept
		{
			return sock->verifyKeyprint(expKP, allowUntrusted);
		}
		
		void setMaxSpeed(int64_t maxSpeed)
		{
			if (hasSocket())
				sock->setMaxSpeed(maxSpeed);
		}
		void updateSocketBucket(int connectionCount, uint64_t tick) const
		{
			if (hasSocket())
				sock->updateSocketBucket(connectionCount, tick);
		}

		void write(const string& data)
		{
			write(data.data(), data.length());
		}
		void write(const void* buf, size_t size);
		void transmitFile(InputStream* stream);
		void setIpVersion(int af) { ipVersion = af; }

		uint16_t getLocalPort() const
		{
			return hasSocket() ? sock->getLocalPort() : 0;
		}

		void start();
		void joinThread() { join(); }
	
	private:
		enum State
		{
			RUNNING,
			STARTING,
			CONNECT_PROXY,
			FAILED
		};

		enum
		{
			TASK_NONE,
			TASK_CONNECT,
			TASK_ACCEPT
		};

		enum
		{
			PROXY_STAGE_NONE,
			PROXY_STAGE_NEGOTIATE,
			PROXY_STAGE_AUTH,
			PROXY_STAGE_CONNECT
		};

		struct Buffer
		{
			Buffer();
			~Buffer();

			uint8_t* buf;
			size_t maxCapacity;
			size_t capacity;
			size_t writePtr;
			size_t readPtr;

			void append(const void* data, size_t size);
			void remove(size_t size) noexcept;
			void grow(size_t newCapacity);
			void grow();
			void shift() noexcept;
			void maybeShift() noexcept;
			void clear() noexcept;
		};

		struct ConnectInfo
		{
			ConnectInfo(const string& addr, uint16_t port, uint16_t localPort, NatRoles natRole, bool secure, bool allowUntrusted, const string& expKP, const Socket::ProxyConfig* proxy) :
				addr(addr), port(port), localPort(localPort), secure(secure), allowUntrusted(allowUntrusted), expKP(expKP), natRole(natRole)
			{
				if (proxy)
				{
					this->proxy = *proxy;
					useProxy = true;
				}
				else
					useProxy = false;
			}
			string addr;
			uint16_t port;
			uint16_t localPort;
			bool secure;
			bool allowUntrusted;
			bool useProxy;
			string expKP;
			NatRoles natRole;
			Socket::ProxyConfig proxy;
		};

		std::unique_ptr<Socket> sock;
		State state;
		int pollMask;
		int pollState;
		Buffer rb;
		Buffer wb;
		Buffer zb;
		Buffer sb;
		CriticalSection cs;
		std::atomic_bool stopFlag;
		Modes mode;
		size_t remainingSize;
		char separator;
		Socket::Protocol protocol;
		std::unique_ptr<UnZFilter> zfilter;
		InputStream* outStream;
		size_t maxLineSize;
		string lineBuf;
		int task;
		int proxyStage;
		uint8_t proxyAuthMethod;
		ConnectInfo* connectInfo;
		uint64_t updateSent;
		uint64_t updateReceived;
		uint64_t gracefulDisconnectTimeout;
		BufferedSocketListener* listener;
		int ipVersion;

		BufferedSocket(char separator, BufferedSocketListener* listener);
		virtual ~BufferedSocket();

		void writeData();
		void readData();
		bool processTask();
		void parseData(Buffer& b);
		void consumeData();
		void decompress();
		void setSocket(std::unique_ptr<Socket>&& s);
		void setOptions();
		void doConnect(const ConnectInfo* ci, bool sslSocks);
		void doAccept();
		void createSocksMessage(const ConnectInfo* ci);
		void checkSocksReply();
		void printSockName(string& sockName) const;

	protected:
		virtual int run() override;
};

#endif // BUFFERED_SOCKET_H_

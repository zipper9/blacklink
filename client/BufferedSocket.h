/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_DCPP_BUFFERED_SOCKET_H
#define DCPLUSPLUS_DCPP_BUFFERED_SOCKET_H

#include <boost/asio/ip/address_v4.hpp>

#include "BufferedSocketListener.h"
#include "Socket.h"
#include "Thread.h"
#include "WinEvent.h"
#include "StrUtil.h"

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
		
		/**
		 * BufferedSocket factory, each BufferedSocket may only be used to create one connection
		 * @param sep Line separator
		 * @return An unconnected socket
		 */
		static BufferedSocket* getBufferedSocket(char sep, BufferedSocketListener* connection)
		{
			return new BufferedSocket(sep, connection);
		}

		static void destroyBufferedSocket(BufferedSocket* sock)
		{
			dcassert(!sock || !sock->isRunning());
			delete sock;
		}
		
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
		static void waitShutdown();
#endif
		
		void addAcceptedSocket(unique_ptr<Socket> newSock, uint16_t port);
		void connect(const string& address, uint16_t port, bool secure, 
			bool allowUntrusted, bool useProxy, Socket::Protocol proto, const string& expKP = Util::emptyString);
		void connect(const string& aAddress, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool secure, 
			bool allowUntrusted, bool useProxy, Socket::Protocol proto, const string& expKP = Util::emptyString);
		
		/** Sets data mode for aBytes bytes. Must be called within onLine. */
		void setDataMode(int64_t bytes = -1)
		{
			mode = MODE_DATA;
			dataBytes = bytes;
		}
		void setMode(Modes newMode) noexcept;
		Modes getMode() const { return mode; }
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
		string getIp() const
		{
			return hasSocket() ? sock->getIp() : Util::emptyString;
		}
		string getRemoteIpPort() const
		{
			return hasSocket() ? sock->getIp() + ':' + Util::toString(sock->getPort()) : Util::emptyString;
		}
		
		boost::asio::ip::address_v4 getIp4() const;
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

		void write(const string& aData)
		{
			write(aData.data(), aData.length());
		}
		void write(const char* aBuf, size_t aLen);
		/** Send the file f over this socket. */
		void transmitFile(InputStream* f)
		{
			addTask(SEND_FILE, new SendFileInfo(f));
		}
		
		/** Send an updated signal to all listeners */
		void updated()
		{
			addTask(UPDATED, nullptr);
		}
		void disconnect(bool graceless = false);
		void shutdown();
		void joinThread() { join(); }

#ifdef FLYLINKDC_USE_DEAD_CODE
		string getLocalIp() const
		{
			return sock->getLocalIp();
		}
#endif
		
		uint16_t getLocalPort() const
		{
			return sock->getLocalPort();
		}
		bool hasSocket() const
		{
			return sock.get() != nullptr;
		}
		bool socketIsDisconnecting() const
		{
			return disconnecting || !hasSocket();
		}

	private:
		string getServerAndPort() const
		{
			return getIp() + ':' + Util::toString(getPort());
		}
		
		Socket::Protocol protocol;
		char separator;
		BufferedSocketListener* listener;
		
		enum Tasks
		{
			CONNECT,
			DISCONNECT,
			SEND_DATA,
			SEND_FILE,
			SHUTDOWN,
			ACCEPTED,
			UPDATED
		};
		
		enum State
		{
			RUNNING,
			STARTING, // Waiting for CONNECT/ACCEPTED/SHUTDOWN
			FAILED
		};
		
		struct TaskData
		{			
			TaskData() { }
			TaskData(const TaskData&) = delete;
			TaskData& operator= (const TaskData&) = delete;
			virtual ~TaskData() { }
		};

		struct ConnectInfo : public TaskData
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

		struct SendFileInfo : public TaskData
		{
			explicit SendFileInfo(InputStream* stream) : stream(stream) { }
			InputStream* stream;
		};
		
		explicit BufferedSocket(char separator, BufferedSocketListener* listener);
		
		~BufferedSocket();
		
		FastCriticalSection cs;
		
		WinEvent<FALSE> semaphore;
		deque<pair<Tasks, std::unique_ptr<TaskData>>> tasks;
		ByteVector inbuf;
		void resizeInBuf();
		ByteVector writeBuf;
		string m_line;
		int64_t dataBytes;
		
		Modes mode;
		State state;
		
		std::unique_ptr<UnZFilter> filterIn;
		std::unique_ptr<Socket> sock;
		string currentLine;
		
		std::atomic_bool disconnecting;
		
		virtual int run() override;
		
		void threadConnect(const ConnectInfo* ci);
		void threadAccept();
		void threadRead();
		void threadSendFile(InputStream* file);
		void threadSendData();
		
		void fail(const string& aError);
		bool checkEvents();
		void checkSocket();
		
		void setSocket(std::unique_ptr<Socket>&& s);
		void setOptions()
		{
			sock->setInBufSize();
			sock->setOutBufSize();
		}
		void addTask(Tasks task, TaskData* data) noexcept;
		bool addTaskL(Tasks task, TaskData* data) noexcept;
};

#endif // !defined(BUFFERED_SOCKET_H)

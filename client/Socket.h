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

#ifndef DCPLUSPLUS_DCPP_SOCKET_H
#define DCPLUSPLUS_DCPP_SOCKET_H

#include "SocketAddr.h"

#ifdef _WIN32

#include <ws2tcpip.h>
#include "WinEvent.h"

#else

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include "PipeEvent.h"

#endif

#include "Exception.h"
#include "BaseUtil.h"

class SocketException : public Exception
{
	public:
#ifdef _DEBUG
		explicit SocketException(const string& error) noexcept :
			Exception("SocketException: " + error), errorCode(0) { }
#else //_DEBUG
		explicit SocketException(const string& error) noexcept :
			Exception(error), errorCode(0) { }
#endif // _DEBUG
		explicit SocketException(int error) noexcept;
		int getErrorCode() const
		{
			return errorCode;
		}

	private:
		static string errorToString(int error) noexcept;
		int errorCode;
};

class Socket
{
	public:
		enum
		{
			WAIT_NONE    = 0x00,
			WAIT_CONNECT = 0x01,
			WAIT_READ    = 0x02,
			WAIT_WRITE   = 0x04,
			WAIT_ACCEPT  = 0x08,
			WAIT_CONTROL = 0x10
		};

		enum SocketType
		{
			TYPE_TCP = 0, // IPPROTO_TCP
			TYPE_UDP = 1  // IPPROTO_UDP
		};

		enum Protocol
		{
			PROTO_DEFAULT = 0,
			PROTO_NMDC    = 1,
			PROTO_ADC     = 2,
			PROTO_HTTP    = 3
		};

		enum SecureTransport
		{
			SECURE_TRANSPORT_NONE,
			SECURE_TRANSPORT_SSL,
			SECURE_TRANSPORT_DETECT
		};

		struct ProxyConfig
		{
			string host;
			uint16_t port;
			string user;
			string password;
			bool resolveNames;
		};

		Socket() : sock(INVALID_SOCKET), connected(false),
			maxSpeed(0), currentBucket(0), bucketUpdateTick(0),
			type(TYPE_TCP), port(0), proto(PROTO_DEFAULT)
		{
			ip.type = 0;
#ifdef _WIN32
			currentMask = 0;
			lastWaitResult = WAIT_WRITE;
#endif
		}

		Socket(const Socket&) = delete;
		Socket& operator= (const Socket&) = delete;

		Socket& operator= (Socket&& src)
		{
			sock = src.sock;
			proto = src.proto;
			type = src.type;
			ip = std::move(src.ip);
			port = src.port;
			maxSpeed = src.maxSpeed;
			currentBucket = src.currentBucket;
			src.sock = INVALID_SOCKET;
			return *this;
		}

		virtual ~Socket()
		{
			disconnect();
			sock = INVALID_SOCKET;
		}

		bool isValid() const { return sock != INVALID_SOCKET; }

		socket_t getSock() const;

		void attachSock(socket_t newSock)
		{
			dcassert(sock == INVALID_SOCKET);
			sock = newSock;
		}

		socket_t detachSock()
		{
			socket_t res = sock;
			sock = INVALID_SOCKET;
			return res;
		}

		// host is used only for logging (empty for literal IPs)
		virtual void connect(const IpAddressEx& ip, uint16_t port, const string& host);
		void connect(const string& host, uint16_t port);

		virtual int write(const void* buffer, int len);
		int write(const string& data)
		{
			return write(data.data(), (int) data.length());
		}
		virtual void shutdown() noexcept;
		virtual void close() noexcept;
		void disconnect() noexcept;

		virtual bool waitConnected(unsigned millis);
		virtual bool waitAccepted(unsigned millis);

		/**
		 * Reads zero to bufLen characters from this socket,
		 * @param buffer A buffer to store the data in.
		 * @param bufLen Size of the buffer.
		 * @return Number of bytes read, 0 if disconnected and -1 if the call would block.
		 * @throw SocketException On any failure.
		 */
		virtual int read(void* buffer, int bufLen);

		/**
		 * Reads data until bufLen bytes have been read or an error occurs.
		 * If the socket is closed, or the timeout is reached, the number of bytes read
		 * actually read is returned.
		 * On exception, an unspecified amount of bytes might have already been read.
		 */
		int readAll(void* buffer, int bufLen, unsigned timeout = 0);

		virtual int sendPacket(const void* buffer, int bufLen, const IpAddress& ip, uint16_t port) noexcept;
		int sendPacket(const IpAddress& addr, uint16_t port, const string& data)
		{
			dcassert(data.length());
			return sendPacket(data.data(), (int) data.length(), addr, port);
		}
		int receivePacket(void* buffer, int bufLen, IpAddress& ip, uint16_t& port) noexcept;

		virtual int wait(int millis, int waitFor);

		void setBlocking(bool block) noexcept;
		uint16_t getLocalPort() const;
		IpAddress getLocalIp() const;

		// Low level interface
		virtual void create(int af, SocketType type);

		/** Binds a socket to a certain local port and possibly IP. */
		virtual uint16_t bind(uint16_t port, const IpAddressEx& addr);
		virtual void listen();
		/** Accept a socket.
		@return remote port */
		virtual uint16_t accept(const Socket& listeningSocket);

		int getSocketOptInt(int level, int option) const;
		void setSocketOpt(int level, int option, int value);
		void setInBufSize();
		void setOutBufSize();

		virtual SecureTransport getSecureTransport() const noexcept
		{
			return SECURE_TRANSPORT_NONE;
		}
		virtual bool isTrusted() const
		{
			return false;
		}
		virtual string getCipherName() const noexcept
		{
			return Util::emptyString;
		}
		virtual vector<uint8_t> getKeyprint() const noexcept
		{
			return Util::emptyByteVector;
		}
		virtual bool verifyKeyprint(const string&, bool /*allowUntrusted*/) noexcept
		{
			return true;
		}

		void setIp(const IpAddress& ip) { this->ip = ip; }
		void setIp(const IpAddressEx& ip);
		const IpAddress& getIp() const { return ip; }

		void setPort(uint16_t port) { this->port = port; }
		uint16_t getPort() const { return port; }

		void setMaxSpeed(int64_t maxSpeed) { this->maxSpeed = maxSpeed; }
		int64_t getMaxSpeed() const { return maxSpeed; }

		void setCurrentBucket(int64_t currentBucket) { this->currentBucket = currentBucket; }
		int64_t getCurrentBucket() const { return currentBucket; }

		void updateSocketBucket(int connectionCount, uint64_t tick)
		{
			if (connectionCount <= 0)
			{
				connectionCount = 1;
				dcassert(0);
			}
			currentBucket = getMaxSpeed() / connectionCount;
			bucketUpdateTick = tick;
		}

		uint64_t getBucketUpdateTick() const { return bucketUpdateTick; }

		static bool getProxyConfig(ProxyConfig& proxy);
		void createControlEvent();
		void signalControlEvent();
		void setConnected() { connected = true; }
		void printSockName(string& s) const;

	protected:
		socket_t sock;
		Protocol proto;
		SocketType type;
		bool connected;
		IpAddress ip;
		uint16_t port;
		int64_t maxSpeed;
		int64_t currentBucket;
		uint64_t bucketUpdateTick;
#ifdef _WIN32
		unsigned lastWaitResult;
#endif

		struct StatsItem
		{
			uint64_t downloaded = 0;
			uint64_t uploaded = 0;
		};

		struct Stats
		{
			StatsItem tcp;
			StatsItem udp;
			StatsItem ssl;
		};

	public:
		static Stats g_stats;
		static int getLastError()
		{
#ifdef _WIN32
			return ::WSAGetLastError();
#else
			return errno;
#endif
		}

	private:
		static socket_t checksocket(socket_t ret)
		{
			if (ret == INVALID_SOCKET)
				throw SocketException(getLastError());
			return ret;
		}
		static void check(int ret, bool blockOk = false)
		{
			if (ret == SOCKET_ERROR)
			{
				const int error = getLastError();
				if (blockOk && (error == SE_EWOULDBLOCK
#ifndef _WIN32
				|| error == EINPROGRESS
#endif
				))
					return;
				throw SocketException(error);
			}
		}

#ifdef _WIN32
		WinEvent<TRUE> event;
		WinEvent<TRUE> controlEvent;
		unsigned currentMask;
#else
		PipeEvent controlEvent;
#endif
};

#endif // DCPLUSPLUS_DCPP_SOCKET_H

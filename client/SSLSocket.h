/*
 * Copyright (C) 2001-2012 Jacek Sieka, arnetheduck on gmail point com
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DCPLUSPLUS_DCPP_SSLSOCKET_H
#define DCPLUSPLUS_DCPP_SSLSOCKET_H

#include <openssl/ssl.h>

#include "CryptoManager.h"
#include "Socket.h"
#include "Singleton.h"

using std::unique_ptr;
using std::string;

class SSLSocketException : public SocketException
{
	public:
#ifdef _DEBUG
		explicit SSLSocketException(const string& error) noexcept :
			SocketException("SSLSocketException: " + error) { }
#else //_DEBUG
		explicit SSLSocketException(const string& error) noexcept :
			SocketException(error) { }
#endif // _DEBUG
		explicit SSLSocketException(int error) noexcept :
			SocketException(error) { }
		virtual ~SSLSocketException() noexcept { }
};

class SSLSocket : public Socket
{
		friend class CryptoManager;
		SSLSocket(SSL_CTX* context, Socket::Protocol proto, bool allowUntrusted, const string& expKP) noexcept;

	public:
		SSLSocket(CryptoManager::SSLContext context, bool allowUntrusted, const string& expKP) noexcept;
		/** Creates an SSL socket without any verification */
		SSLSocket(CryptoManager::SSLContext context) noexcept;
		
		virtual ~SSLSocket()
		{
			disconnect();
		}
		
		virtual uint16_t accept(const Socket& listeningSocket) override;
		virtual void connect(const IpAddressEx& ip, uint16_t port, const string& host) override;
		virtual int read(void* buffer, int bufLen) override;
		virtual int write(const void* buffer, int len) override;
		virtual int wait(int millis, int waitFor) override;
		virtual void shutdown() noexcept override;
		virtual void close() noexcept override;
		
		virtual SecureTransport getSecureTransport() const noexcept override
		{
			return SECURE_TRANSPORT_SSL;
		}
		virtual bool isTrusted() const override;
		virtual string getCipherName() const noexcept override;
		virtual ByteVector getKeyprint() const noexcept override;
		virtual bool verifyKeyprint(const string& expKeyp, bool allowUntrusted) noexcept override;
		
		virtual bool waitConnected(unsigned millis) override;
		virtual bool waitAccepted(unsigned millis) override;

		void setServerName(const string& name) { serverName = name; }

	private:
		SSL_CTX* ctx;
		ssl::SSL ssl;
		Socket::Protocol nextProto;
		mutable bool isTrustedCached;
		string serverName;
		
		unique_ptr<CryptoManager::SSLVerifyData> verifyData;    // application data used by CryptoManager::verify_callback(...)
		
		int checkSSL(int ret);
		bool waitWant(int ret, unsigned millis);
		void logInfo(bool isServer) const;
};

#endif // SSLSOCKET_H

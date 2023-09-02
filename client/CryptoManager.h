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

#ifndef DCPLUSPLUS_DCPP_CRYPTO_MANAGER_H
#define DCPLUSPLUS_DCPP_CRYPTO_MANAGER_H

#include "SettingsManager.h"

#include "Exception.h"
#include "Singleton.h"
#include "Socket.h"

#include <openssl/ssl.h>

namespace ssl
{

#define DECLARE_SSL_CLASS(_name, _free) \
	class _name : public std::unique_ptr<::_name, decltype(&_free)> \
	{ \
		public: \
		_name(::_name *x = nullptr) : std::unique_ptr<::_name, decltype(&_free)>(x, _free) {} \
		operator ::_name*() { return get(); } \
		operator const ::_name*() const { return get(); } \
	};

DECLARE_SSL_CLASS(BIO, BIO_free)
DECLARE_SSL_CLASS(SSL, SSL_free)
DECLARE_SSL_CLASS(SSL_CTX, SSL_CTX_free)

DECLARE_SSL_CLASS(ASN1_INTEGER, ASN1_INTEGER_free)
DECLARE_SSL_CLASS(BIGNUM, BN_free)
DECLARE_SSL_CLASS(DH, DH_free)

DECLARE_SSL_CLASS(DSA, DSA_free)
DECLARE_SSL_CLASS(EVP_PKEY, EVP_PKEY_free)
DECLARE_SSL_CLASS(RSA, RSA_free)
DECLARE_SSL_CLASS(X509, X509_free)
DECLARE_SSL_CLASS(X509_NAME, X509_NAME_free)

}

#ifndef SSL_SUCCESS
#define SSL_SUCCESS 1
#endif

STANDARD_EXCEPTION(CryptoException);

class SSLSocket;

class CryptoManager : public Singleton<CryptoManager>
{
	public:
		typedef pair<bool, string> SSLVerifyData;

		enum TLSTmpKeys
		{
			KEY_DH_2048,
			KEY_DH_4096,
			NUM_KEYS
		};

		enum SSLContext
		{
			SSL_CLIENT,
			SSL_SERVER,
			SSL_UNAUTH_CLIENT,
			MAX_CONTEXT
		};

		SSLSocket* getClientSocket(bool allowUntrusted, const string& expKP, Socket::Protocol proto) noexcept;
		SSLSocket* getServerSocket(bool allowUntrusted) noexcept;

		SSL_CTX* getSSLContext(SSLContext wanted) const noexcept;

		bool isInitialized() const noexcept;
		bool initializeKeyPair() noexcept;
		void generateNewKeyPair();
		void getCertFingerprint(ByteVector& fp) const noexcept;
		void checkExpiredCert() noexcept;

		static int idxVerifyData;

	private:
		friend class Singleton<CryptoManager>;

		CryptoManager();
		virtual ~CryptoManager();

		bool loadKeyPair(const string& certFile, const string& keyFile, ssl::X509& cert, ssl::EVP_PKEY& pkey) noexcept;
		void createKeyPair(const string& certFile, const string& keyFile, ssl::X509& cert, ssl::EVP_PKEY& pkey);
		bool loadOrCreateKeyPair(ssl::X509& cert, ssl::EVP_PKEY& pkey, bool createOnError) noexcept;
		static void sslRandCheck() noexcept;
		static ssl::SSL_CTX createContext(bool isServer) noexcept;
		static StringList loadTrustedList() noexcept;
		static void loadVerifyLocations(ssl::SSL_CTX& ctx, const StringList& files) noexcept;

		static int getKeyLength(TLSTmpKeys key);
		static DH* getTmpDH(int keyLen);
		static DH* getTempDHCallback(SSL* /*ssl*/, int /*is_export*/, int keylength);
		static int verifyCallback(int preverifyOk, X509_STORE_CTX *ctx);

#if OPENSSL_VERSION_NUMBER < 0x10100000
		static void lockFunc(int mode, int n, const char *file, int line);
		static CriticalSection* cs;
#endif

		static string formatError(X509_STORE_CTX *ctx, const string& message);
		static string getNameEntryByNID(X509_NAME* name, int nid) noexcept;
		static int64_t getX509EndTime(const X509* cert) noexcept;

	public:
		static void getX509Digest(ByteVector& out, const X509* x509, const EVP_MD* md) noexcept;

	private:
		ssl::SSL_CTX context[MAX_CONTEXT];
		mutable CriticalSection contextLock;
		bool keyPairInitialized;
		ByteVector certFingerprint;
		int64_t endTime;

		static SSLVerifyData trustedKeyprint;
};

#endif // !defined(CRYPTO_MANAGER_H)

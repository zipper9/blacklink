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
			KEY_FIRST = 0,
			KEY_DH_2048 = KEY_FIRST,
			KEY_DH_4096,
			KEY_RSA_2048,
			KEY_LAST
		};
		
		enum SSLContext
		{
			SSL_CLIENT,
			SSL_CLIENT_ALPN,
			SSL_SERVER
		};

		SSLSocket* getClientSocket(bool allowUntrusted, const string& expKP, Socket::Protocol proto);
		SSLSocket* getServerSocket(bool allowUntrusted);
		
		SSL_CTX* getSSLContext(SSLContext wanted);
		
		void loadCertificates(bool createOnError = true) noexcept;
		void generateCertificate();
		static const ByteVector& getKeyprint() noexcept;
		
		static bool TLSOk() noexcept;
		
		static int verify_callback(int preverifyOk, X509_STORE_CTX *ctx);
		static DH* tmp_dh_cb(SSL* /*ssl*/, int /*is_export*/, int keylength);
		static RSA* tmp_rsa_cb(SSL* /*ssl*/, int /*is_export*/, int keylength);
		static void locking_function(int mode, int n, const char *file, int line);
		
		static int idxVerifyData;
		
	private:
		friend class Singleton<CryptoManager>;
		
		CryptoManager();
		virtual ~CryptoManager();
		
		ssl::SSL_CTX clientContext;
		ssl::SSL_CTX clientALPNContext;
		ssl::SSL_CTX serverContext;
		ssl::SSL_CTX serverALPNContext;
		
		bool load(const string& certFile, const string& keyFile, ssl::X509& cert, ssl::EVP_PKEY& pkey) noexcept;
		void sslRandCheck();
		
		static int getKeyLength(TLSTmpKeys key);
		static DH* getTmpDH(int keyLen);
		static RSA* getTmpRSA(int keyLen);
		
		static bool certsLoaded;
		
		static CriticalSection* cs;
		static char idxVerifyDataName[];
		static SSLVerifyData trustedKeyprint;
		
		static ByteVector keyprint;
		
		static string formatError(X509_STORE_CTX *ctx, const string& message);
		static string getNameEntryByNID(X509_NAME* name, int nid) noexcept;

	public:
		static ByteVector X509_digest_internal(::X509* x509, const ::EVP_MD* md);
};

#endif // !defined(CRYPTO_MANAGER_H)

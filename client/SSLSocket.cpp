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

#include "stdinc.h"
#include "SSLSocket.h"
#include "LogManager.h"
#include "SettingsManager.h"
#include "ResourceManager.h"
#include "CryptoManager.h"
#include "ParamExpander.h"
#include "StrUtil.h"
#include "TimeUtil.h"
#include "Util.h"
#include "ConfCore.h"
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
static const unsigned char alpnNMDC[] = { 4, 'n', 'm', 'd', 'c' };
static const unsigned char alpnADC[]  = { 3, 'a', 'd', 'c' };
#ifdef USE_HTTP_ALPN
static const unsigned char alpnHTTP[] = { 8, 'h', 't', 't', 'p', '/', '1', '.', '1' };
#endif
#endif

SSLSocket::SSLSocket(SSL_CTX* context, Socket::Protocol proto, bool allowUntrusted, const string& expKP) noexcept : ctx(context), ssl(nullptr), nextProto(proto), isTrustedCached(false)
{
	verifyData.reset(new CryptoManager::SSLVerifyData(allowUntrusted, expKP));
}

SSLSocket::SSLSocket(CryptoManager::SSLContext context, bool allowUntrusted, const string& expKP) noexcept : SSLSocket(context)
{
	verifyData.reset(new CryptoManager::SSLVerifyData(allowUntrusted, expKP));
}

SSLSocket::SSLSocket(CryptoManager::SSLContext context) noexcept : ctx(nullptr), ssl(nullptr), verifyData(nullptr), isTrustedCached(false)
{
	ctx = CryptoManager::getInstance()->getSSLContext(context);
}

void SSLSocket::connect(const IpAddressEx& ip, uint16_t port, const string& host)
{
	Socket::connect(ip, port, host);
	waitConnected(0);
}

#if OPENSSL_VERSION_NUMBER < 0x10002000L
static inline int SSL_is_server(SSL *s)
{
	return s->server;
}
#endif

#ifdef _WIN32
static long sslReadCallback(BIO *b, int oper, const char *argp, size_t len, int argi, long argl, int ret, size_t *processed) noexcept
{
	if (oper == BIO_CB_READ)
	{
		char* arg = BIO_get_callback_arg(b);
		if (arg)
			reinterpret_cast<SSLSocket*>(arg)->clearPendingRead();
	}
	return ret;
}

void SSLSocket::clearPendingRead() noexcept
{
	lastWaitResult &= ~WAIT_READ;
}
#endif

bool SSLSocket::waitConnected(unsigned millis)
{
	if (!ssl)
	{
		if (!Socket::waitConnected(millis))
			return false;
		ssl.reset(SSL_new(ctx));
		if (!ssl)
			checkSSL(-1);

		if (!verifyData)
			SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);
		else
			SSL_set_ex_data(ssl, CryptoManager::idxVerifyData, verifyData.get());

		checkSSL(SSL_set_fd(ssl, static_cast<int>(getSock())));
#ifdef _WIN32
		BIO* bio = SSL_get_rbio(ssl);
		BIO_set_callback_ex(bio, sslReadCallback);
		BIO_set_callback_arg(bio, reinterpret_cast<char*>(this));
#endif
		if (!serverName.empty())
			SSL_set_tlsext_host_name(ssl, serverName.c_str());
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
		if (nextProto == Socket::PROTO_NMDC)
		{
			SSL_set_alpn_protos(ssl, alpnNMDC, sizeof(alpnNMDC));
		}
		else if (nextProto == Socket::PROTO_ADC)
		{
			SSL_set_alpn_protos(ssl, alpnADC, sizeof(alpnADC));
		}
#ifdef USE_HTTP_ALPN
		else if (nextProto == Socket::PROTO_HTTP)
		{
			SSL_set_alpn_protos(ssl, alpnHTTP, sizeof(alpnHTTP));
		}
#endif
#endif
	}

	if (SSL_is_init_finished(ssl))
		return true;

	while (true)
	{
		int isServer = SSL_is_server(ssl);
		int ret = isServer ? SSL_accept(ssl) : SSL_connect(ssl);
		if (ret == 1)
		{
			logInfo(isServer);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
			if (isServer) return true;
			const unsigned char* protocol = 0;
			unsigned int len = 0;
			SSL_get0_alpn_selected(ssl, &protocol, &len);
			if (len != 0)
			{
				if (len == 3 && !memcmp(protocol, alpnADC + 1, len))
					proto = PROTO_ADC;
				else if (len == 4 && !memcmp(protocol, alpnNMDC + 1, len))
					proto = PROTO_NMDC;
				dcdebug("ALPN negotiated %.*s (%d)\n", len, protocol, proto);
			}
#endif
#ifdef _WIN32
			BIO* bio = SSL_get_rbio(ssl);
			BIO_set_callback_ex(bio, nullptr);
			BIO_set_callback_arg(bio, nullptr);
#endif
			return true;
		}
		uint64_t startTick = Util::getTick();
		if (!waitWant(ret, millis)) return false;
		uint64_t waited = Util::getTick() - startTick;
		if (waited >= millis) return false;
		millis -= (unsigned) waited;
	}
}

uint16_t SSLSocket::accept(const Socket& listeningSocket)
{
	auto ret = Socket::accept(listeningSocket);
	waitAccepted(0);
	return ret;
}

bool SSLSocket::waitAccepted(unsigned millis)
{
	if (!ssl)
	{
		if (!Socket::waitAccepted(millis))
			return false;
		ssl.reset(SSL_new(ctx));
		if (!ssl)
			checkSSL(-1);

		if (!verifyData)
			SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);
		else
			SSL_set_ex_data(ssl, CryptoManager::idxVerifyData, verifyData.get());

		checkSSL(SSL_set_fd(ssl, static_cast<int>(getSock())));
#ifdef _WIN32
		BIO* bio = SSL_get_rbio(ssl);
		BIO_set_callback_ex(bio, sslReadCallback);
		BIO_set_callback_arg(bio, reinterpret_cast<char*>(this));
#endif
	}

	if (SSL_is_init_finished(ssl))
		return true;

	while (true)
	{
		int ret = SSL_accept(ssl);
		if (ret == 1)
		{
			logInfo(true);
			dcdebug("SSLSocket accepted using %s\n", SSL_get_cipher(ssl));
#ifdef _WIN32
			BIO* bio = SSL_get_rbio(ssl);
			BIO_set_callback_ex(bio, nullptr);
			BIO_set_callback_arg(bio, nullptr);
#endif
			return true;
		}
		uint64_t startTick = Util::getTick();
		if (!waitWant(ret, millis)) return false;
		uint64_t waited = Util::getTick() - startTick;
		if (waited >= millis) return false;
		millis -= (unsigned) waited;
	}
}

bool SSLSocket::waitWant(int ret, unsigned millis)
{
	int err = SSL_get_error(ssl, ret);
	switch (err)
	{
		case SSL_ERROR_WANT_READ:
			return wait(millis, Socket::WAIT_READ) == WAIT_READ;
		case SSL_ERROR_WANT_WRITE:
			return wait(millis, Socket::WAIT_WRITE) == WAIT_WRITE;
	}
	// Check if this is a fatal error...
	checkSSL(ret);
	return false;
}

int SSLSocket::read(void* buffer, int size)
{
	if (!ssl)
		return -1;
#ifdef _WIN32
	lastWaitResult &= ~WAIT_READ;
#endif
	int len = checkSSL(SSL_read(ssl, buffer, size));

	if (len > 0)
		g_stats.ssl.downloaded += len;
	return len;
}

int SSLSocket::write(const void* buffer, int size)
{
	if (!ssl)
		return -1;
	int ret = 0;
	if (size)
	{
		ret = checkSSL(SSL_write(ssl, buffer, size));
		if (ret < 0)
		{
#ifdef _WIN32
			lastWaitResult &= ~WAIT_WRITE;
#endif
		}
		else
			g_stats.ssl.uploaded += ret;
		if (ret > 0)
			g_stats.ssl.uploaded += ret;
	}
	return ret;
}

int SSLSocket::checkSSL(int ret)
{
	if (!ssl)
		return -1;
	if (ret <= 0)
	{
		/* inspired by boost.asio (asio/ssl/detail/impl/engine.ipp, function engine::perform) and
		the SSL_get_error doc at <https://www.openssl.org/docs/ssl/SSL_get_error.html>. */
		auto err = SSL_get_error(ssl, ret);
		switch (err)
		{
			case SSL_ERROR_NONE:
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				return -1;
			case SSL_ERROR_ZERO_RETURN:
				throw SocketException(STRING(CONNECTION_CLOSED));
			case SSL_ERROR_SYSCALL:
			{
				auto sysErr = ERR_get_error();
				if (sysErr == 0)
				{
					if (ret == 0)
					{
						dcdebug("TLS error: call ret = %d, SSL_get_error = %d, ERR_get_error = %lu\n", ret, err, sysErr);
						throw SSLSocketException(STRING(CONNECTION_CLOSED));
					}
					sysErr = getLastError();
				}
				throw SSLSocketException(sysErr);
			}
			default:
			{
				auto sysErr = ERR_get_error();
#ifdef SSL_R_UNEXPECTED_EOF_WHILE_READING
				int errLib = ERR_GET_LIB(sysErr);
				int errReason = ERR_GET_REASON(sysErr);
				// This happens when server resets the connection.
				if (errLib == ERR_LIB_SSL && errReason == SSL_R_UNEXPECTED_EOF_WHILE_READING)
					throw SocketException(STRING(CONNECTION_CLOSED));
#endif
				string details;
				//display the cert errors as first choice, if the error is not the certs display the error from the ssl.
				int verifyResult = SSL_get_verify_result(ssl);
				if (verifyResult == X509_V_ERR_APPLICATION_VERIFICATION)
					details = "Keyprint mismatch";
				else if (verifyResult != X509_V_OK)
					details = X509_verify_cert_error_string(verifyResult);
				else
					details = ERR_error_string(sysErr, nullptr);
				ssl.reset();
				//dcdebug("TLS error: call ret = %d, SSL_get_error = %d, ERR_get_error = " U64_FMT ",ERROR string: %s \n", ret, err, sysErr, ERR_error_string(sysErr, nullptr));
				if (details.empty())
					details = STRING(TLS_ERROR);
				else
					details.insert(0, STRING(TLS_ERROR) + ": ");
				throw SSLSocketException(details);
			}
		}
	}
	return ret;
}

int SSLSocket::wait(int millis, int waitFor)
{
	if (ssl && (waitFor & Socket::WAIT_READ))
	{
		/** @todo Take writing into account as well if reading is possible? */
		if (SSL_pending(ssl) > 0)
			return WAIT_READ;
	}
	return Socket::wait(millis, waitFor);
}

bool SSLSocket::isTrusted() const
{
	if (!ssl)
		return false;
	if (isTrustedCached)
		return true;
	if (SSL_get_verify_result(ssl) != X509_V_OK)
		return false;

	X509* cert = SSL_get_peer_certificate(ssl);
	if (!cert)
		return false;
	X509_free(cert);
	isTrustedCached = true;
	return true;
}

std::string SSLSocket::getCipherName() const noexcept
{
	if (!ssl)
		return Util::emptyString;
		
	const string cipher = SSL_get_cipher_name(ssl);
	return cipher;
}

ByteVector SSLSocket::getKeyprint() const noexcept
{
	if (!ssl)
		return ByteVector();
		
	X509* x509 = SSL_get_peer_certificate(ssl);
	
	if (!x509)
		return ByteVector();

	ByteVector res;
	CryptoManager::getX509Digest(res, x509, EVP_sha256());

	X509_free(x509);
	return res;
}

bool SSLSocket::verifyKeyprint(const string& expKP, bool allowUntrusted) noexcept
{
	if (!ssl)
		return true;
		
	if (expKP.empty() || expKP.find('/') == string::npos)
		return allowUntrusted;
		
	verifyData.reset(new CryptoManager::SSLVerifyData(allowUntrusted, expKP));
	SSL_set_ex_data(ssl, CryptoManager::idxVerifyData, verifyData.get());
	
	X509_STORE* store = X509_STORE_new();
	
	bool result = false;
	int err = SSL_get_verify_result(ssl);
	if (store)
	{
		X509_STORE_CTX* vrfy_ctx = X509_STORE_CTX_new();
		X509* cert = SSL_get_peer_certificate(ssl);
		
		if (vrfy_ctx && cert && X509_STORE_CTX_init(vrfy_ctx, store, cert, SSL_get_peer_cert_chain(ssl)))
		{
			X509_STORE_CTX_set_ex_data(vrfy_ctx, SSL_get_ex_data_X509_STORE_CTX_idx(), ssl);
			X509_STORE_CTX_set_verify_cb(vrfy_ctx, SSL_get_verify_callback(ssl));
			
			int verify_result = 0;
			if ((verify_result = X509_verify_cert(vrfy_ctx)) >= 0)
			{
				err = X509_STORE_CTX_get_error(vrfy_ctx);
				
				// Watch out for weird library errors that might not set the context error code
				if (err == X509_V_OK && verify_result <= 0)
					err = X509_V_ERR_UNSPECIFIED;
					
				// This is for people who don't restart their clients and have low expiration time on their cert
				result = (err == X509_V_OK || err == X509_V_ERR_CERT_HAS_EXPIRED) || (allowUntrusted && err != X509_V_ERR_APPLICATION_VERIFICATION);
			}
		}
		
		if (cert) X509_free(cert);
		if (vrfy_ctx) X509_STORE_CTX_free(vrfy_ctx);
		X509_STORE_free(store);
	}
	
	// KeyPrint is a strong indicator of trust
	SSL_set_verify_result(ssl, err);
	
	return result;
}

void SSLSocket::shutdown() noexcept
{
	isTrustedCached = false;
	if (ssl)
		SSL_shutdown(ssl);
}

void SSLSocket::close() noexcept
{
	isTrustedCached = false;
	if (ssl)
	{
		ssl.reset();
	}
	Socket::shutdown();
	Socket::close();
}

void SSLSocket::logInfo(bool isServer) const
{
	int options = LogManager::getLogOptions();
	if (options & LogManager::OPT_LOG_SOCKET_INFO)
	{
		string logText;
		string ip = Util::printIpAddress(getIp(), true);
		if (isServer)
			logText = "SSL: accepted connection from " + ip + " using " + SSL_get_cipher(ssl) + ", sock=" + Util::toHexString(getSock());
		else
			logText = "SSL: connected to " + ip + " using " + SSL_get_cipher(ssl) + ", sock=" + Util::toHexString(getSock());
		LogManager::message(logText, false);
		if (options & LogManager::OPT_LOG_CERTIFICATES)
		{
			auto ss = SettingsManager::instance.getCoreSettings();
			ss->lockRead();
			const string fileFormat = ss->getString(Conf::LOG_FILE_TLS_CERT);
			string outPath = ss->getString(Conf::LOG_DIRECTORY);
			ss->unlockRead();
			if (!fileFormat.empty())
			{
				X509* cert = SSL_get_peer_certificate(ssl);
				if (cert)
				{
					ByteVector digest;
					CryptoManager::getX509Digest(digest, cert, EVP_sha256());
					StringMap m;
					m["IP"] = m["ip"] = Util::printIpAddress(getIp());
					m["KP"] = m["kp"] = Util::toBase32(digest.data(), digest.size());
					outPath += Util::validateFileName(Util::formatParams(fileFormat, m, true));
					if (!File::isExist(outPath))
					{
						unsigned char *data = nullptr;
						int size = ASN1_item_i2d((ASN1_VALUE*) cert, &data, ASN1_ITEM_rptr(X509));
						if (size)
						{
							try
							{
								File::ensureDirectory(outPath);
								File f(outPath, File::WRITE, File::CREATE | File::TRUNCATE, true);
								f.write(data, size);
							}
							catch (Exception&)
							{
							}
						}
						OPENSSL_free(data);
					}
					X509_free(cert);
				}
			}
		}
	}
}

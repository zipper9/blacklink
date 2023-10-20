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
#include "CryptoManager.h"

#include "File.h"
#include "LogManager.h"
#include "ClientManager.h"
#include "TimeUtil.h"
#include "version.h"

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "SSLSocket.h"

static void* tmpKeysMap[CryptoManager::NUM_KEYS] = { nullptr, nullptr };

int CryptoManager::idxVerifyData = 0;
CryptoManager::SSLVerifyData CryptoManager::trustedKeyprint = { false, "trusted_keyp" };
static CriticalSection g_cs;

#if OPENSSL_VERSION_NUMBER < 0x10101000
#define USE_ASN1_TIME_TO_TM
#endif

#ifdef USE_ASN1_TIME_TO_TM
static bool asn1_time_to_tm(tm& t, const ASN1_TIME* d, bool enforceRfc5280);
#endif

// Cipher suites for TLS <= 1.2
static const char cipherSuites[] =
	"ECDHE-ECDSA-AES128-GCM-SHA256:"
	"ECDHE-RSA-AES128-GCM-SHA256:"
	"ECDHE-ECDSA-AES256-GCM-SHA384:"
	"ECDHE-RSA-AES256-GCM-SHA384:"
	"ECDHE-ECDSA-AES128-SHA256:"
	"ECDHE-RSA-AES128-SHA256:"
	"ECDHE-ECDSA-AES256-SHA384:"
	"ECDHE-RSA-AES256-SHA384:"
	"ECDHE-ECDSA-AES128-SHA:"
	"ECDHE-RSA-AES128-SHA:"
	"ECDHE-ECDSA-AES256-SHA:"
	"ECDHE-RSA-AES256-SHA:"
	"DHE-RSA-AES128-GCM-SHA256:"
	"DHE-RSA-AES128-SHA256:"
	"DHE-RSA-AES128-SHA:"
	"AES128-GCM-SHA256:"
	"AES256-GCM-SHA384:"
	"AES128-SHA256:"
	"AES256-SHA256:"
	"AES128-SHA";

static const int64_t EXPIRED_CERT_TIME_DIFF = 3600; // seconds

CryptoManager::CryptoManager()
{
	keyPairInitialized = false;
	endTime = 0;
#if OPENSSL_VERSION_NUMBER < 0x10100000
	int n = CRYPTO_num_locks();
	cs = new CriticalSection[n];
	CRYPTO_set_locking_callback(lockFunc);

	SSL_library_init();
	SSL_load_error_strings();
#else
	OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
#endif

	sslRandCheck();
	idxVerifyData = SSL_get_ex_new_index(0, (void *) "VerifyData", nullptr, nullptr, nullptr);

	// Init temp data for DH keys
	for (int i = 0; i < NUM_KEYS; ++i)
		tmpKeysMap[i] = getTmpDH(getKeyLength(static_cast<TLSTmpKeys>(i)));

	context[SSL_UNAUTH_CLIENT] = createContext(false);
}

CryptoManager::~CryptoManager()
{
	for (int i = 0; i < NUM_KEYS; ++i)
		if (tmpKeysMap[i]) DH_free(static_cast<DH*>(tmpKeysMap[i]));

#if OPENSSL_VERSION_NUMBER < 0x10100000
	ERR_remove_thread_state(nullptr);
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();

	CRYPTO_set_locking_callback(nullptr);
	delete[] cs;
#endif
}

void CryptoManager::sslRandCheck() noexcept
{
	if (!RAND_status())
	{
#ifndef _WIN32
		if (!File::isExist("/dev/urandom"))
		{
			// This is questionable, but hopefully we don't end up here
			time_t t = time(nullptr);
			RAND_seed(&t, sizeof(time_t));
			pid_t pid = getpid();
			RAND_seed(&pid, sizeof(pid_t));
			char stackdata[1024];
			RAND_seed(&stackdata, 1024);
		}
#else
		RAND_screen();
#endif
	}
}

ssl::SSL_CTX CryptoManager::createContext(bool isServer) noexcept
{
	auto ctxPtr = ssl::SSL_CTX(SSL_CTX_new(isServer ? SSLv23_server_method() : SSLv23_client_method()));
	if (ctxPtr)
	{
		SSL_CTX* ctx = ctxPtr.get();
		unsigned long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
		if (isServer) options |= SSL_OP_SINGLE_DH_USE;
		SSL_CTX_set_options(ctx, options);
		SSL_CTX_set_cipher_list(ctx, cipherSuites);
#if 0 // Don't set curves list, just use the defaults
		SSL_CTX_set1_curves_list(ctx, "P-256");
#endif

		if (isServer)
		{
			EC_KEY* ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
			if (ec)
			{
				SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);
				SSL_CTX_set_tmp_ecdh(ctx, ec);
				EC_KEY_free(ec);
			}
			SSL_CTX_set_tmp_dh_callback(ctx, getTempDHCallback);
		}

		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyCallback);
#if OPENSSL_VERSION_NUMBER >= 0x30000000
		SSL_CTX_set_security_level(ctx, 0); // Required to enable SHA1
#endif
	}
	return ctxPtr;
}

string CryptoManager::formatError(X509_STORE_CTX *ctx, const string& message)
{
	LOCK(g_cs);
	X509* cert = nullptr;
	if ((cert = X509_STORE_CTX_get_current_cert(ctx)) != nullptr)
	{
		X509_NAME* subject = X509_get_subject_name(cert);
		string tmp, line;

		tmp = getNameEntryByNID(subject, NID_commonName);
		if (!tmp.empty())
		{
			if (tmp.length() < 39)
				tmp.append(39 - tmp.length(), 'x');
			CID certCID(tmp);
			if (tmp.length() == 39 && !certCID.isZero())
				tmp = Util::toString(ClientManager::getNicks(certCID, Util::emptyString, false));
			line += (!line.empty() ? ", " : "") + tmp;
		}

		tmp = getNameEntryByNID(subject, NID_organizationName);
		if (!tmp.empty())
		{
			if (!line.empty()) line += ", ";
			line += tmp;
		}
		if (line.empty()) line = "<Empty Name>";

		return str(F_("Certificate verification for %1% failed with error: %2%") % line % message);
	}

	return Util::emptyString;
}

int CryptoManager::verifyCallback(int preverifyOk, X509_STORE_CTX *ctx)
{
	int err = X509_STORE_CTX_get_error(ctx);
	SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	SSLVerifyData* verifyData = (SSLVerifyData*)SSL_get_ex_data(ssl, CryptoManager::idxVerifyData);

	// This happens only when KeyPrint has been pinned and we are not skipping errors due to incomplete chains
	// we can fail here f.ex. if the certificate has expired but is still pinned with KeyPrint
	if (!verifyData || SSL_get_shutdown(ssl) != 0)
		return preverifyOk;

	bool allowUntrusted = verifyData->first;
	string keyp = verifyData->second;
	string error = Util::emptyString;

	if (!keyp.empty())
	{
		X509* cert = X509_STORE_CTX_get_current_cert(ctx);
		if (!cert)
			return 0;

		if (keyp.compare(0, 12, "trusted_keyp") == 0)
		{
			// Possible follow up errors, after verification of a partial chain
			if (err == X509_V_ERR_CERT_UNTRUSTED || err == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE)
			{
				X509_STORE_CTX_set_error(ctx, X509_V_OK);
				return 1;
			}

			return preverifyOk;
		}
		else if (keyp.compare(0, 7, "SHA256/") != 0)
			return allowUntrusted ? 1 : 0;

		ByteVector kp;
		getX509Digest(kp, cert, EVP_sha256());
		string expected_keyp = "SHA256/" + Util::toBase32(kp.data(), kp.size());

		// Do a full string comparison to avoid potential false positives caused by invalid inputs
		if (keyp.compare(expected_keyp) == 0)
		{
			// KeyPrint validated, we can get rid of it (to avoid unnecessary passes)
			SSL_set_ex_data(ssl, CryptoManager::idxVerifyData, nullptr);

			if (err != X509_V_OK)
			{
#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
				X509_STORE* store = X509_STORE_CTX_get0_store(ctx);
#else
				X509_STORE* store = ctx->ctx;
#endif

				// Hide the potential library error about trying to add a dupe
				ERR_set_mark();
				if (X509_STORE_add_cert(store, cert))
				{
					// We are fine, but can't leave mark on the stack
					ERR_pop_to_mark();

					// After the store has been updated, perform a *complete* recheck of the peer certificate, the existing context can be in mid recursion, so hands off!
					X509_STORE_CTX* vrfy_ctx = X509_STORE_CTX_new();

					if (vrfy_ctx && X509_STORE_CTX_init(vrfy_ctx, store, cert, X509_STORE_CTX_get_chain(ctx)))
					{
						// Welcome to recursion hell... it should at most be 2n, where n is the number of certificates in the chain
						X509_STORE_CTX_set_ex_data(vrfy_ctx, SSL_get_ex_data_X509_STORE_CTX_idx(), ssl);
						X509_STORE_CTX_set_verify_cb(vrfy_ctx, SSL_get_verify_callback(ssl));

						int verify_result = X509_verify_cert(vrfy_ctx);
						if (verify_result >= 0)
						{
							err = X509_STORE_CTX_get_error(vrfy_ctx);

							// Watch out for weird library errors that might not set the context error code
							if (err == X509_V_OK && verify_result == 0)
								err = X509_V_ERR_UNSPECIFIED;
						}
					}

					X509_STORE_CTX_set_error(ctx, err); // Set the current cert error to the context being verified.
					if (vrfy_ctx) X509_STORE_CTX_free(vrfy_ctx);
				}
				else ERR_pop_to_mark();

				// KeyPrint was not root certificate or we don't have the issuer certificate, the best we can do is trust the pinned KeyPrint
				if (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN || err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY || err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT)
				{
					X509_STORE_CTX_set_error(ctx, X509_V_OK);
					// Set this to allow ignoring any follow up errors caused by the incomplete chain
					SSL_set_ex_data(ssl, CryptoManager::idxVerifyData, &CryptoManager::trustedKeyprint);
					return 1;
				}
			}

			if (err == X509_V_OK)
				return 1;

			// We are still here, something went wrong, complain below...
			preverifyOk = 0;
		}
		else
		{
			if (X509_STORE_CTX_get_error_depth(ctx) > 0)
				return 1;

			// TODO: How to get this into HubFrame?
			preverifyOk = 0;
			err = X509_V_ERR_APPLICATION_VERIFICATION;
#ifndef _DEBUG
			error = "Supplied KeyPrint did not match any certificate.";
#else
			error = str(F_("Supplied KeyPrint %1% did not match %2%.") % keyp % expected_keyp);
#endif

			X509_STORE_CTX_set_error(ctx, err);
		}
	}

	// We let untrusted certificates through unconditionally, when allowed, but we like to complain regardless
	if (!preverifyOk)
	{
		if (error.empty())
			error = X509_verify_cert_error_string(err);

		auto fullError = formatError(ctx, error);
		if (!fullError.empty() && (!keyp.empty() || !allowUntrusted))
			LogManager::message(fullError);
	}

	// Don't allow untrusted connections on keyprint mismatch
	if (allowUntrusted && err != X509_V_ERR_APPLICATION_VERIFICATION)
		return 1;

	return preverifyOk;
}

int CryptoManager::getKeyLength(TLSTmpKeys key)
{
	switch (key)
	{
		case KEY_DH_2048:
			return 2048;
		case KEY_DH_4096:
			return 4096;
		default:
			dcassert(0);
			return 0;
	}
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
	dh->p = p;
	dh->q = q;
	dh->g = g;

	return 1;
}
#endif

DH* CryptoManager::getTmpDH(int keyLen)
{
	if (keyLen < 2048)
		return nullptr;

	DH* tmpDH = DH_new();
	if (!tmpDH) return nullptr;

	static const unsigned char dh_g[] =	{ 0x02 };

	// From RFC 3526; checked via http://wiki.openssl.org/index.php/Diffie-Hellman_parameters#Validating_Parameters
	switch (keyLen)
	{
		case 2048:
		{
			static const unsigned char dh2048_p[] =
			{
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
				0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
				0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
				0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
				0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
				0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
				0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
				0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
				0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
				0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
				0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 0x98, 0xDA, 0x48, 0x36,
				0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
				0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56,
				0x20, 0x85, 0x52, 0xBB, 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
				0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 0xF1, 0x74, 0x6C, 0x08,
				0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
				0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2,
				0xEC, 0x07, 0xA2, 0x8F, 0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
				0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, 0x39, 0x95, 0x49, 0x7C,
				0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
				0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68, 0xFF, 0xFF, 0xFF, 0xFF,
				0xFF, 0xFF, 0xFF, 0xFF,
			};
			DH_set0_pqg(tmpDH, BN_bin2bn(dh2048_p, sizeof(dh2048_p), 0), nullptr, BN_bin2bn(dh_g, sizeof(dh_g), 0));
			break;
		}

		case 4096:
		{
			static const unsigned char dh4096_p[] =
			{
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
				0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
				0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
				0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
				0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
				0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
				0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
				0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
				0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
				0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
				0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 0x98, 0xDA, 0x48, 0x36,
				0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
				0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56,
				0x20, 0x85, 0x52, 0xBB, 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
				0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 0xF1, 0x74, 0x6C, 0x08,
				0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
				0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2,
				0xEC, 0x07, 0xA2, 0x8F, 0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
				0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, 0x39, 0x95, 0x49, 0x7C,
				0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
				0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAA, 0xC4, 0x2D, 0xAD, 0x33, 0x17, 0x0D,
				0x04, 0x50, 0x7A, 0x33, 0xA8, 0x55, 0x21, 0xAB, 0xDF, 0x1C, 0xBA, 0x64,
				0xEC, 0xFB, 0x85, 0x04, 0x58, 0xDB, 0xEF, 0x0A, 0x8A, 0xEA, 0x71, 0x57,
				0x5D, 0x06, 0x0C, 0x7D, 0xB3, 0x97, 0x0F, 0x85, 0xA6, 0xE1, 0xE4, 0xC7,
				0xAB, 0xF5, 0xAE, 0x8C, 0xDB, 0x09, 0x33, 0xD7, 0x1E, 0x8C, 0x94, 0xE0,
				0x4A, 0x25, 0x61, 0x9D, 0xCE, 0xE3, 0xD2, 0x26, 0x1A, 0xD2, 0xEE, 0x6B,
				0xF1, 0x2F, 0xFA, 0x06, 0xD9, 0x8A, 0x08, 0x64, 0xD8, 0x76, 0x02, 0x73,
				0x3E, 0xC8, 0x6A, 0x64, 0x52, 0x1F, 0x2B, 0x18, 0x17, 0x7B, 0x20, 0x0C,
				0xBB, 0xE1, 0x17, 0x57, 0x7A, 0x61, 0x5D, 0x6C, 0x77, 0x09, 0x88, 0xC0,
				0xBA, 0xD9, 0x46, 0xE2, 0x08, 0xE2, 0x4F, 0xA0, 0x74, 0xE5, 0xAB, 0x31,
				0x43, 0xDB, 0x5B, 0xFC, 0xE0, 0xFD, 0x10, 0x8E, 0x4B, 0x82, 0xD1, 0x20,
				0xA9, 0x21, 0x08, 0x01, 0x1A, 0x72, 0x3C, 0x12, 0xA7, 0x87, 0xE6, 0xD7,
				0x88, 0x71, 0x9A, 0x10, 0xBD, 0xBA, 0x5B, 0x26, 0x99, 0xC3, 0x27, 0x18,
				0x6A, 0xF4, 0xE2, 0x3C, 0x1A, 0x94, 0x68, 0x34, 0xB6, 0x15, 0x0B, 0xDA,
				0x25, 0x83, 0xE9, 0xCA, 0x2A, 0xD4, 0x4C, 0xE8, 0xDB, 0xBB, 0xC2, 0xDB,
				0x04, 0xDE, 0x8E, 0xF9, 0x2E, 0x8E, 0xFC, 0x14, 0x1F, 0xBE, 0xCA, 0xA6,
				0x28, 0x7C, 0x59, 0x47, 0x4E, 0x6B, 0xC0, 0x5D, 0x99, 0xB2, 0x96, 0x4F,
				0xA0, 0x90, 0xC3, 0xA2, 0x23, 0x3B, 0xA1, 0x86, 0x51, 0x5B, 0xE7, 0xED,
				0x1F, 0x61, 0x29, 0x70, 0xCE, 0xE2, 0xD7, 0xAF, 0xB8, 0x1B, 0xDD, 0x76,
				0x21, 0x70, 0x48, 0x1C, 0xD0, 0x06, 0x91, 0x27, 0xD5, 0xB0, 0x5A, 0xA9,
				0x93, 0xB4, 0xEA, 0x98, 0x8D, 0x8F, 0xDD, 0xC1, 0x86, 0xFF, 0xB7, 0xDC,
				0x90, 0xA6, 0xC0, 0x8F, 0x4D, 0xF4, 0x35, 0xC9, 0x34, 0x06, 0x31, 0x99,
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			};
			DH_set0_pqg(tmpDH, BN_bin2bn(dh4096_p, sizeof(dh4096_p), 0), nullptr, BN_bin2bn(dh_g, sizeof(dh_g), 0));
			break;
		}

		default:
			DH_free(tmpDH);
			return nullptr;

	}

	return tmpDH;
}

bool CryptoManager::isInitialized() const noexcept
{
	if (!BOOLSETTING(USE_TLS)) return false;
	LOCK(contextLock);
	return keyPairInitialized && !certFingerprint.empty();
}

#ifdef _WIN32
static inline FILE* create_utf8(const string& s)
{
	return _wfopen(Text::utf8ToWide(s).c_str(), L"w");
}
#else
static inline FILE* create_utf8(const string& s)
{
	return fopen(s.c_str(), "w");
}
#endif

static bool loadFile(const string& file, int64_t maxSize, ByteVector& data) noexcept
{
	try
	{
		File f(file, File::READ, File::OPEN);
		int64_t size = f.getSize();
		if (size <= 0 || size > maxSize) return false;
		size_t len = (size_t) size;
		data.resize(len);
		size_t requiredLen = len;
		if (f.read(&data[0], len) != requiredLen) return false;
	}
	catch (Exception&)
	{
		return false;
	}
	return true;
}

int64_t CryptoManager::getX509EndTime(const X509* cert) noexcept
{
	const ASN1_TIME* at = X509_get_notAfter(cert);
	if (!at)
		return 0;
#ifdef USE_ASN1_TIME_TO_TM
	tm t;
	return asn1_time_to_tm(t, at, false) ? Util::gmtToUnixTime(&t) : 0;
#else
	int days = 0, seconds = 0;
	ASN1_TIME* t0 = ASN1_TIME_new();
	ASN1_TIME_set(t0, 0);
	int64_t result = 0;
	if (ASN1_TIME_diff(&days, &seconds, t0, at))
		result = (int64_t) days * 86400 + seconds;
	ASN1_TIME_free(t0);
	return result;
#endif
}

bool CryptoManager::loadKeyPair(const string& certFile, const string& keyFile, ssl::X509& cert, ssl::EVP_PKEY& pkey) noexcept
{
	static const int64_t maxFileSize = 128 * 1024;
	ByteVector data;
	if (!loadFile(certFile, maxFileSize, data))
		return false;

	ssl::BIO bioCert(BIO_new_mem_buf(data.data(), data.size()));
	if (!bioCert)
		return false;

	cert.reset(PEM_read_bio_X509(bioCert, nullptr, nullptr, nullptr));
	if (!cert)
		return false;

	if (!loadFile(keyFile, maxFileSize, data))
		return false;

	ssl::BIO bioKey(BIO_new_mem_buf(data.data(), data.size()));
	if (!bioKey)
		return false;

	pkey.reset(PEM_read_bio_PrivateKey(bioKey, nullptr, nullptr, nullptr));
	if (!pkey)
		return false;

	const ASN1_INTEGER* sn = X509_get_serialNumber(cert);
	if (!ASN1_INTEGER_get(sn))
		return false;

	X509_NAME* name = X509_get_subject_name(cert);
	if (!name)
		return false;

	const string cn = getNameEntryByNID(name, NID_commonName);
	if (cn.empty() || cn != ClientManager::getInstance()->getMyCID().toBase32())
		return false;

	int64_t currentTime = (int64_t) time(nullptr);
	int64_t endTime = getX509EndTime(cert);
	return endTime - currentTime >= EXPIRED_CERT_TIME_DIFF;
}

void CryptoManager::createKeyPair(const string& certFile, const string& keyFile, ssl::X509& cert, ssl::EVP_PKEY& pkey)
{
	cert.reset(X509_new());
	pkey.reset(EVP_PKEY_new());

	ssl::BIGNUM bn(BN_new());
	ssl::RSA rsa(RSA_new());
	ssl::X509_NAME nm(X509_NAME_new());
	ssl::ASN1_INTEGER serial(ASN1_INTEGER_new());

	if (!cert || !pkey || !bn || !rsa || !serial || !nm)
		throw CryptoException("Error creating objects for cert generation");

	long days = 90;
	int keylength = 2048;

#define CHECK(n) if (!(n)) { throw CryptoException(#n); }

	// Generate key pair
	CHECK((BN_set_word(bn, RSA_F4)))
	CHECK((RSA_generate_key_ex(rsa, keylength, bn, nullptr)))
	CHECK((EVP_PKEY_set1_RSA(pkey, rsa)))

	// Add common name (use cid)
	string name = ClientManager::getInstance()->getMyCID().toBase32();
	CHECK((X509_NAME_add_entry_by_NID(nm, NID_commonName, MBSTRING_ASC, (unsigned char *) name.c_str(), name.length(), -1, 0)))

	// Add an organisation
	static const char *org = "DCPlusPlus (OSS/SelfSigned)";
	CHECK((X509_NAME_add_entry_by_NID(nm, NID_organizationName, MBSTRING_ASC, (unsigned char *) org, strlen(org), -1, 0)))

	// Generate unique serial
	CHECK((BN_pseudo_rand(bn, 64, 0, 0)))
	CHECK((BN_to_ASN1_INTEGER(bn, serial)))

	// Prepare self-signed cert
	CHECK((X509_set_version(cert, 0x02)))
	CHECK((X509_set_serialNumber(cert, serial)))
	CHECK((X509_set_issuer_name(cert, nm)))
	CHECK((X509_set_subject_name(cert, nm)))
	CHECK((X509_gmtime_adj(X509_get_notBefore(cert), 0)))
	CHECK((X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24 * days)))
	CHECK((X509_set_pubkey(cert, pkey)))

	// Sign using own private key
	CHECK((X509_sign(cert, pkey, EVP_sha256())))

#undef CHECK

	// Write the key and cert
	{
		File::ensureDirectory(keyFile);
		FILE* f = create_utf8(keyFile);
		if (!f)
			throw CryptoException(STRING_F(ERROR_CREATING_FILE, keyFile));
		PEM_write_RSAPrivateKey(f, rsa, nullptr, nullptr, 0, nullptr, nullptr);
		fclose(f);
	}
	{
		File::ensureDirectory(certFile);
		FILE* f = create_utf8(certFile);
		if (!f)
		{
			File::deleteFile(keyFile);
			throw CryptoException(STRING_F(ERROR_CREATING_FILE, certFile));
		}
		PEM_write_X509(f, cert);
		fclose(f);
	}
}

bool CryptoManager::loadOrCreateKeyPair(ssl::X509& cert, ssl::EVP_PKEY& pkey, bool createOnError) noexcept
{
	const string& certFile = SETTING(TLS_CERTIFICATE_FILE);
	const string& keyFile = SETTING(TLS_PRIVATE_KEY_FILE);

	if (certFile.empty() || keyFile.empty())
	{
		LogManager::message(STRING(NO_CERTIFICATE_FILE_SET));
		return false;
	}

	if (!loadKeyPair(certFile, keyFile, cert, pkey))
	{
		if (!createOnError)
		{
			if (!cert)
				LogManager::message(STRING(FAILED_TO_LOAD_CERTIFICATE));
			else if (!pkey)
				LogManager::message(STRING(FAILED_TO_LOAD_PRIVATE_KEY));
			return false;
		}
		try
		{
			createKeyPair(certFile, keyFile, cert, pkey);
			LogManager::message(STRING(CERTIFICATE_GENERATED));
		}
		catch (const CryptoException& e)
		{
			LogManager::message(STRING(CERTIFICATE_GENERATION_FAILED) + " " + e.getError());
			return false;
		}
	}
	return true;
}

void CryptoManager::generateNewKeyPair()
{
	const string& keyFile = SETTING(TLS_PRIVATE_KEY_FILE);
	if (keyFile.empty())
		throw CryptoException("No private key file chosen");

	const string& certFile = SETTING(TLS_CERTIFICATE_FILE);
	if (certFile.empty())
		throw CryptoException("No certificate file chosen");

	ssl::X509 cert;
	ssl::EVP_PKEY pkey;
	createKeyPair(certFile, keyFile, cert, pkey);

	StringList files = loadTrustedList();

	ssl::SSL_CTX newContext[2];
	for (int i = 0; i < 2; i++)
	{
		newContext[i] = createContext(i == SSL_SERVER);
		if (!newContext[i] ||
			SSL_CTX_use_certificate(newContext[i].get(), cert) != SSL_SUCCESS ||
			SSL_CTX_use_PrivateKey(newContext[i].get(), pkey) != SSL_SUCCESS)
			throw CryptoException("Failed to initialize SSL context");
		loadVerifyLocations(newContext[i], files);
	}

	ByteVector digest;
	getX509Digest(digest, cert, EVP_sha256());
	int64_t endTime = getX509EndTime(cert);

	LOCK(contextLock);
	for (int i = 0; i < 2; i++)
		context[i] = std::move(newContext[i]);
	certFingerprint = digest;
	this->endTime = endTime;
	keyPairInitialized = true;
}

bool CryptoManager::initializeKeyPair() noexcept
{
	ssl::X509 cert;
	ssl::EVP_PKEY pkey;

	if (!loadOrCreateKeyPair(cert, pkey, true))
	{
		LOCK(contextLock);
		keyPairInitialized = false;
		return false;
	}

	StringList files = loadTrustedList();

	ssl::SSL_CTX newContext[2];
	for (int i = 0; i < 2; i++)
	{
		newContext[i] = createContext(i == SSL_SERVER);
		if (!newContext[i] ||
			SSL_CTX_use_certificate(newContext[i].get(), cert) != SSL_SUCCESS ||
			SSL_CTX_use_PrivateKey(newContext[i].get(), pkey) != SSL_SUCCESS)
		{
			LogManager::message("Failed to initialize SSL context");
			LOCK(contextLock);
			keyPairInitialized = false;
			return false;
		}
		loadVerifyLocations(newContext[i], files);
	}

	ByteVector digest;
	getX509Digest(digest, cert, EVP_sha256());
	int64_t endTime = getX509EndTime(cert);

	LOCK(contextLock);
	for (int i = 0; i < 2; i++)
		context[i] = std::move(newContext[i]);
	certFingerprint = digest;
	this->endTime = endTime;
	keyPairInitialized = true;
	return true;
}

StringList CryptoManager::loadTrustedList() noexcept
{
	const string& path = SETTING(TLS_TRUSTED_CERTIFICATES_PATH);
	if (path.empty()) return StringList();
	StringList certs = File::findFiles(path, "*.pem", true);
	StringList certs2 = File::findFiles(path, "*.crt", true);
	certs.insert(certs.end(), certs2.begin(), certs2.end());
	return certs;
}

void CryptoManager::loadVerifyLocations(ssl::SSL_CTX& ctx, const StringList& files) noexcept
{
	for (const string& file : files)
	{
		if (SSL_CTX_load_verify_locations(ctx.get(), file.c_str(), nullptr) != SSL_SUCCESS)
			LogManager::message("Failed to load trusted certificate from " + file);
	}
}

string CryptoManager::getNameEntryByNID(X509_NAME* name, int nid) noexcept
{
	int i = X509_NAME_get_index_by_NID(name, nid, -1);
	if (i == -1)
		return Util::emptyString;

	X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, i);
	ASN1_STRING* str = X509_NAME_ENTRY_get_data(entry);
	if (!str)
		return Util::emptyString;

	unsigned char* buf = 0;
	i = ASN1_STRING_to_UTF8(&buf, str);
	if (i < 0)
		return Util::emptyString;

	std::string out((char*)buf, i);
	OPENSSL_free(buf);

	return out;
}

void CryptoManager::getCertFingerprint(ByteVector& fp) const noexcept
{
	LOCK(contextLock);
	fp = certFingerprint;
}

SSL_CTX* CryptoManager::getSSLContext(SSLContext wanted) const noexcept
{
	LOCK(contextLock);
	SSL_CTX* ctx = context[wanted].get();
	if (ctx)
#if OPENSSL_VERSION_NUMBER < 0x10100000
		CRYPTO_add(&ctx->references, 1, CRYPTO_LOCK_SSL_CTX);
#else
		SSL_CTX_up_ref(ctx);
#endif
	return ctx;
}

SSLSocket* CryptoManager::getClientSocket(bool allowUntrusted, const string& expKP, Socket::Protocol proto) noexcept
{
	SSL_CTX* ctx = getSSLContext(SSL_CLIENT);
	return ctx ? new SSLSocket(ctx, proto, allowUntrusted, expKP) : nullptr;
}

SSLSocket* CryptoManager::getServerSocket(bool allowUntrusted) noexcept
{
	SSL_CTX* ctx = getSSLContext(SSL_SERVER);
	return ctx ? new SSLSocket(ctx, Socket::PROTO_DEFAULT, allowUntrusted, Util::emptyString) : nullptr;
}

DH* CryptoManager::getTempDHCallback(SSL* /*ssl*/, int /*is_export*/, int keylength)
{
	if (keylength < 2048)
		return static_cast<DH*>(tmpKeysMap[KEY_DH_2048]);

	void* tmpDH = nullptr;
	switch (keylength)
	{
		case 2048:
			tmpDH = tmpKeysMap[KEY_DH_2048];
			break;
		case 4096:
			tmpDH = tmpKeysMap[KEY_DH_4096];
			break;
	}

	return static_cast<DH*>(tmpDH ? tmpDH : tmpKeysMap[KEY_DH_2048]);
}

#if OPENSSL_VERSION_NUMBER < 0x10100000
CriticalSection* CryptoManager::cs = nullptr;

void CryptoManager::lockFunc(int mode, int n, const char* /*file*/, int /*line*/)
{
	if (mode & CRYPTO_LOCK)
		cs[n].lock();
	else
		cs[n].unlock();
}
#endif

void CryptoManager::getX509Digest(ByteVector& out, const X509* x509, const EVP_MD* md) noexcept
{
	unsigned n;
	out.resize(EVP_MAX_MD_SIZE);
	if (!X509_digest(x509, md, out.data(), &n))
		out.clear();
	else
		out.resize(n);
}

void CryptoManager::checkExpiredCert() noexcept
{
	int64_t currentTime = time(nullptr);
	{
		LOCK(contextLock);
		if (!endTime || endTime - currentTime >= EXPIRED_CERT_TIME_DIFF)
			return;
	}
	try { generateNewKeyPair(); }
	catch (Exception&) {}
}

#ifdef USE_ASN1_TIME_TO_TM
static inline bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

static inline bool isLeapYear(int year) noexcept
{
	if (year % 400 == 0) return true;
	if (year % 100 == 0) return false;
	return year % 4 == 0;
}

// Copied from ossl_asn1_time_to_tm
bool asn1_time_to_tm(tm& t, const ASN1_TIME* d, bool enforceRfc5280)
{
	static const int8_t min[9] = { 0, 0, 1, 1, 0, 0, 0, 0, 0 };
	static const int8_t max[9] = { 99, 99, 12, 31, 23, 59, 59, 12, 59 };
	static const int8_t mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	int n, i, i2, o, min_l = 11, end = 6, btz = 5, md;
	bool strict = false;
	/*
	 * ASN1_STRING_FLAG_X509_TIME is used to enforce RFC 5280
	 * time string format, in which:
	 *
	 * 1. "seconds" is a 'MUST'
	 * 2. "Zulu" timezone is a 'MUST'
	 * 3. "+|-" is not allowed to indicate a time zone
	 */
	if (d->type == V_ASN1_UTCTIME)
	{
		if (enforceRfc5280)
		{
			min_l = 13;
			strict = true;
		}
	}
	else if (d->type == V_ASN1_GENERALIZEDTIME)
	{
		end = 7;
		btz = 6;
		if (enforceRfc5280)
		{
			min_l = 15;
			strict = true;
		}
		else
			min_l = 13;
	}
	else
		return false;

	int l = d->length;
	const char* a = (const char *) d->data;
	o = 0;
	memset(&t, 0, sizeof(t));

	/*
	 * GENERALIZEDTIME is similar to UTCTIME except the year is represented
	 * as YYYY. This stuff treats everything as a two digit field so make
	 * first two fields 00 to 99
	 */

	if (l < min_l)
		goto err;
	for (i = 0; i < end; i++)
	{
		if (!strict && i == btz && (a[o] == 'Z' || a[o] == '+' || a[o] == '-'))
		{
			i++;
			break;
		}
		if (!isDigit(a[o]))
			goto err;
		n = a[o] - '0';
		/* incomplete 2-digital number */
		if (++o == l)
			goto err;

		if (!isDigit(a[o]))
			goto err;
		n = (n * 10) + a[o] - '0';
		/* no more bytes to read, but we haven't seen time-zone yet */
		if (++o == l)
			goto err;

		i2 = (d->type == V_ASN1_UTCTIME) ? i + 1 : i;

		if (n < min[i2] || n > max[i2])
			goto err;
		switch (i2)
		{
			case 0:
				/* UTC will never be here */
				t.tm_year = n * 100 - 1900;
				break;
			case 1:
				if (d->type == V_ASN1_UTCTIME)
					t.tm_year = n < 50 ? n + 100 : n;
				else
					t.tm_year += n;
				break;
			case 2:
				t.tm_mon = n - 1;
				break;
			case 3:
				/* check if tm_mday is valid in tm_mon */
				if (t.tm_mon == 1) /* it's February */
					md = mdays[1] + isLeapYear(t.tm_year + 1900);
				else
					md = mdays[t.tm_mon];
				if (n > md)
					goto err;
				t.tm_mday = n;
#if 0 // NOTE: tm_wday and tm_yday are not needed
				determine_days(&t);
#endif
				break;
			case 4:
				t.tm_hour = n;
				break;
			case 5:
				t.tm_min = n;
				break;
			case 6:
				t.tm_sec = n;
				break;
		}
	}

	/*
	 * Optional fractional seconds: decimal point followed by one or more
	 * digits.
	 */
	if (d->type == V_ASN1_GENERALIZEDTIME && a[o] == '.')
	{
		if (strict)
			/* RFC 5280 forbids fractional seconds */
			goto err;
		if (++o == l)
			goto err;
		i = o;
		while (o < l && isDigit(a[o]))
			o++;
		/* Must have at least one digit after decimal point */
		if (i == o)
			goto err;
		/* no more bytes to read, but we haven't seen time-zone yet */
		if (o == l)
			goto err;
	}

	/*
	 * 'o' will never point to '\0' at this point, the only chance
	 * 'o' can point to '\0' is either the subsequent if or the first
	 * else if is true.
	 */
	if (a[o] == 'Z')
		o++;
	else if (!strict && (a[o] == '+' || a[o] == '0'))
	{
		int offsign = a[o] == '-' ? 1 : -1;
		int offset = 0;
		o++;
		/*
		 * if not equal, no need to do subsequent checks
		 * since the following for-loop will add 'o' by 4
		 * and the final return statement will check if 'l'
		 * and 'o' are equal.
		 */
		if (o + 4 != l)
			goto err;
		for (i = end; i < end + 2; i++)
		{
			if (!isDigit(a[o]))
				goto err;
			n = a[o] - '0';
			o++;
			if (!isDigit(a[o]))
				goto err;
			n = (n * 10) + a[o] - '0';
			i2 = (d->type == V_ASN1_UTCTIME) ? i + 1 : i;
			if (n < min[i2] || n > max[i2])
				goto err;
			if (i == end)
				offset = n * 3600;
			else if (i == end + 1)
				offset += n * 60;
			o++;
		}
		if (offset) // NOTE: TZ offset not supported
			goto err;
//		if (offset && !OPENSSL_gmtime_adj(&tmp, 0, offset * offsign))
//			goto err;
	}
	else
		goto err; /* not Z, or not +/- in non-strict mode */
	if (o == l)
		return true;
 err:
	return false;
}
#endif

#include "stdinc.h"
#include "WebServerAuth.h"
#include "Base32.h"

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#else
#error OpenSSL is required to compile this file
#endif

WebServerAuth::WebServerAuth() noexcept
{
	memset(authKey, 0, sizeof(authKey));
}

#pragma pack(1)
struct AuthCookie
{
	uint8_t iv[WebServerAuth::AC_IV_SIZE];
	uint64_t contextId;
	uint64_t expires;
	uint8_t tag[WebServerAuth::AC_TAG_SIZE];
};
#pragma pack()

static const size_t ENC_DATA_SIZE = sizeof(AuthCookie) - (WebServerAuth::AC_IV_SIZE + WebServerAuth::AC_TAG_SIZE);

string WebServerAuth::createAuthCookie(uint64_t expires, uint64_t contextId, const unsigned char* iv) const noexcept
{
	union
	{
		AuthCookie ac;
		unsigned char data[sizeof(AuthCookie)];
	} u;
	unsigned char tmp[sizeof(AuthCookie)];
	memcpy(u.ac.iv, iv, AC_IV_SIZE);
	u.ac.contextId = contextId;
	u.ac.expires = expires;
	auto ctx = EVP_CIPHER_CTX_new();
	EVP_CipherInit_ex(ctx, EVP_aes_128_gcm(), nullptr, authKey, u.ac.iv, 1);
	int outLen = ENC_DATA_SIZE;
	EVP_EncryptUpdate(ctx, u.data + AC_IV_SIZE, &outLen, u.data + AC_IV_SIZE, ENC_DATA_SIZE);
	outLen = sizeof(tmp);
	EVP_EncryptFinal(ctx, tmp, &outLen);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AC_TAG_SIZE, u.ac.tag);
	EVP_CIPHER_CTX_free(ctx);
	return Util::toBase32(u.data, sizeof(AuthCookie));
}

bool WebServerAuth::checkAuthCookie(const string& cookie, uint64_t timestamp, uint64_t& contextId) const noexcept
{
	union
	{
		AuthCookie ac;
		unsigned char data[sizeof(AuthCookie)];
	} u;
	static const size_t BASE32_SIZE = (sizeof(AuthCookie) * 8 + 4) / 5;
	if (cookie.length() != BASE32_SIZE) return false;
	bool error = false;
	Util::fromBase32(cookie.data(), u.data, sizeof(AuthCookie), &error);
	if (error) return false;

	unsigned char tmp[sizeof(AuthCookie)];
	auto ctx = EVP_CIPHER_CTX_new();
	EVP_CipherInit_ex(ctx, EVP_aes_128_gcm(), nullptr, authKey, u.ac.iv, 0);
	int outLen = ENC_DATA_SIZE;
	EVP_DecryptUpdate(ctx, u.data + AC_IV_SIZE, &outLen, u.data + AC_IV_SIZE, ENC_DATA_SIZE);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AC_TAG_SIZE, u.ac.tag);
	outLen = sizeof(tmp);
	bool result = false;
	if (EVP_DecryptFinal(ctx, tmp, &outLen))
	{
		contextId = u.ac.contextId;
		result = timestamp < u.ac.expires;
	}
	EVP_CIPHER_CTX_free(ctx);
	return result;
}

void WebServerAuth::initAuthSecret() noexcept
{
	RAND_bytes(authKey, sizeof(authKey));
}

void WebServerAuth::createAuthIV(unsigned char* iv) noexcept
{
	RAND_bytes(iv, AC_IV_SIZE);
}

void WebServerAuth::updateAuthIV(unsigned char* iv) noexcept
{
	for (size_t i = 0; i < AC_IV_SIZE; ++i)
		if (++iv[i]) break;
}

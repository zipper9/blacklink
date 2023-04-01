#ifndef WEB_SERVER_AUTH_H_
#define WEB_SERVER_AUTH_H_

#include "typedefs.h"

class WebServerAuth
{
public:
	static const size_t AC_IV_SIZE  = 12;
	static const size_t AC_TAG_SIZE = 10;

	WebServerAuth() noexcept;
	void initAuthSecret() noexcept;
	bool checkAuthCookie(const string& cookie, uint64_t timestamp, uint64_t& contextId) const noexcept;
	string createAuthCookie(uint64_t expires, uint64_t contextId, const unsigned char* iv) const noexcept;
	static void createAuthIV(unsigned char* iv) noexcept;
	static void updateAuthIV(unsigned char* iv) noexcept;

private:
	unsigned char authKey[16];
};

#endif // WEB_SERVER_AUTH_H_

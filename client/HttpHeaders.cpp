#include "stdinc.h"
#include "HttpHeaders.h"

/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf -C --ignore-case -I -L ANSI-C -G -m 64 headers.txt  */
/* Computed positions: -k'1,$' */

#include <string.h>

#define TOTAL_KEYWORDS 44
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 19
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 57
/* maximum key range = 55, duplicates = 0 */

static inline int asciiToLower(int c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
	return c;
}

static bool equal(const char *s1, const char *s2, unsigned len) noexcept
{
	for (unsigned i = 0; i < len; i++)
	{
		char c1 = asciiToLower(s1[i]);
		char c2 = asciiToLower(s2[i]);
		if (c1 != c2) return false;
	}
	return true;
}

static unsigned int hash(const char *str, unsigned int len) noexcept
{
	static const unsigned char asso_values[] =
	{
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58,  0, 58,  0,  5,  0,
		58, 13, 23, 17, 58, 11, 28, 58, 11, 58,
		27, 58,  0, 25, 12,  0, 58, 35, 58, 58,
		58, 58, 58, 58, 58, 58, 58,  0, 58,  0,
		5,  0, 58, 13, 23, 17, 58, 11, 28, 58,
		11, 58, 27, 58,  0, 25, 12,  0, 58, 35,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58
	};
	return len + asso_values[(unsigned char)str[len - 1]] + asso_values[(unsigned char)str[0]];
}

static const char* wordList[] =
{
	"", "", "",
	"Age",
	"",
	"Range",
	"Cookie",
	"Referer",
	"",
	"Date",
	"",
	"Retry-After",
	"Content-Type",
	"Content-Range",
	"TE",
	"Accept-Language",
	"Content-Language",
	"ETag",
	"Accept",
	"Trailer",
	"",
	"Connection",
	"User-Agent",
	"",
	"Authorization",
	"If-Range",
	"Accept-Charset",
	"Content-Location",
	"Accept-Encoding",
	"Content-Encoding",
	"Content-Disposition",
	"Server",
	"Expires",
	"Pragma",
	"If-Modified-Since",
	"Set-Cookie",
	"If-Unmodified-Since",
	"Content-Length",
	"Accept-Ranges",
	"Host",
	"Allow",
	"Cache-Control",
	"Transfer-Encoding",
	"Link",
	"",
	"Proxy-Authenticate",
	"Last-Modified",
	"Location",
	"If-Match",
	"", "",
	"WWW-Authenticate",
	"",
	"If-None-Match",
	"",
	"Warning",
	"",
	"Proxy-Authorization"
};

int Http::getHeaderId(const char* str, unsigned len) noexcept
{
	if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
	{
		int key = hash(str, len);
		if (key <= MAX_HASH_VALUE && key >= 0)
		{
			const char *s = wordList[key];
			if ((((unsigned char)*str ^ (unsigned char)*s) & ~32) == 0 && equal(str, s, len))
				return key;
		}
	}
	return -1;
}

int Http::getHeaderId(const string& s) noexcept
{
	return getHeaderId(s.c_str(), s.length());
}

const char* Http::getHeader(int id) noexcept
{
	if (id <= MAX_HASH_VALUE && id >= 0)
	{
		const char *s = wordList[id];
		if (*s) return s;
	}
	return nullptr;
}

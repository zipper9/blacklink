#ifndef TAG16_H_
#define TAG16_H_

#include <string>
#include <stdint.h>
#include <boost/predef/other/endian.h>

#if BOOST_ENDIAN_BIG_BYTE
#define TAG(x, y) ((uint16_t) y | ((uint16_t) x << 8))
#else
#define TAG(x, y) ((uint16_t) x | ((uint16_t) y << 8))
#endif

static inline std::string tagToString(uint16_t tag)
{
	union
	{
		char c[2];
		uint16_t ui;
	} v;
	v.ui = tag;
	return std::string(v.c, 2);
}

#endif // TAG16_H_

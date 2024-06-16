#ifndef USER_TYPE_COLORS_H_
#define USER_TYPE_COLORS_H_

#include "../client/OnlineUser.h"

namespace UserTypeColors
{
	enum
	{
		IS_FAVORITE         = 0x0003 << 2,
		IS_FAVORITE_ON      = 0x0001 << 2,
		IS_BAN              = 0x0003 << 4,
		IS_BAN_ON           = 0x0001 << 4,
		IS_RESERVED_SLOT    = 0x0003 << 6,
		IS_RESERVED_SLOT_ON = 0x0001 << 6,
		IS_IGNORED_USER     = 0x0003 << 8,
		IS_IGNORED_USER_ON  = 0x0001 << 8
	};

	COLORREF getColor(unsigned short& flags, const OnlineUserPtr& onlineUser);
	COLORREF getColor(unsigned short& flags, const UserPtr& user);
}

#endif // USER_TYPE_COLORS_H_

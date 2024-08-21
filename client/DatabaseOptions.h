#ifndef DB_OPTIONS_H_
#define DB_OPTIONS_H_

namespace DatabaseOptions
{
	enum
	{
		ENABLE_IP_STAT       = 0x01,
		ENABLE_USER_STAT     = 0x02,
		USE_CUSTOM_LOCATIONS = 0x04,
		ENABLE_P2P_GUARD     = 0x08,
		P2P_GUARD_BLOCK      = 0x10
	};
}

#endif // DB_OPTIONS_H_

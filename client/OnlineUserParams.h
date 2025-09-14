#ifndef ONLINE_USER_PARAMS_H_
#define ONLINE_USER_PARAMS_H_

#include "Ip4Address.h"
#include "Ip6Address.h"

struct OnlineUserParams
{
	int64_t bytesShared;
	int slots;
	int limit;
	Ip4Address ip4;
	Ip6Address ip6;
	std::string tag;
	std::string nick;
};

#endif // ONLINE_USER_PARAMS_H_

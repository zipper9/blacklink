#ifndef _NODEADDRESS_H
#define _NODEADDRESS_H

#include "../typedefs.h"
#include <boost/functional/hash.hpp>

namespace dht
{
	struct NodeAddress
	{
		uint32_t ip;
		uint16_t port;

		NodeAddress() {}
		NodeAddress(uint32_t ip, uint16_t port) : ip(ip), port(port) {}
	};

	inline bool operator==(const NodeAddress& a, const NodeAddress& b)
	{
		return a.ip == b.ip && a.port == b.port;
	}
}

namespace boost
{
	template<>
	struct hash<dht::NodeAddress>
	{
		size_t operator()(const dht::NodeAddress& na) const
		{
			size_t seed = 0;
			boost::hash_combine(seed, na.ip);
			boost::hash_combine(seed, na.port);
			return seed;
		}
	};
}

#endif // _NODEADDRESS_H

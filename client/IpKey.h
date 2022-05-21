#ifndef IP_KEY_H_
#define IP_KEY_H_

#include "IpAddress.h"

struct IpKey
{
	union
	{
		uint8_t b[16];
		uint32_t dw[4];
	} u;

	bool operator< (const IpKey& x) const
	{
		for (int i = 0; i < 4; i++)
			if (u.dw[i] != x.u.dw[i])
				return u.dw[i] < x.u.dw[i];
		return false;
	}
	bool operator== (const IpKey& x) const
	{
		return u.dw[0] == x.u.dw[0] && u.dw[1] == x.u.dw[1] &&
		       u.dw[2] == x.u.dw[2] && u.dw[3] == x.u.dw[3];
	}
	void setIP(Ip4Address ip)
	{
		u.dw[0] = 0;
		u.dw[1] = 0;
		u.dw[2] = htonl(0xFFFF);
		u.dw[3] = htonl(ip);
	}
	void setIP(const Ip6Address& ip)
	{
		memcpy(u.b, ip.data, 16);
	}
	uint32_t getHash() const
	{
		return u.dw[0] ^ u.dw[1] ^ u.dw[2] ^ u.dw[3];
	}
};

struct IpPortKey
{
	union
	{
		uint8_t b[16];
		uint32_t dw[4];
	} u;
	uint16_t port;

	bool operator< (const IpPortKey& x) const
	{
		for (int i = 0; i < 4; i++)
			if (u.dw[i] != x.u.dw[i])
				return u.dw[i] < x.u.dw[i];
		return port < x.port;
	}
	bool operator== (const IpPortKey& x) const
	{
		return u.dw[0] == x.u.dw[0] && u.dw[1] == x.u.dw[1] &&
		       u.dw[2] == x.u.dw[2] && u.dw[3] == x.u.dw[3] &&
		       port == x.port;
	}
	void setIP(Ip4Address ip, uint16_t port)
	{
		u.dw[0] = 0;
		u.dw[1] = 0;
		u.dw[2] = htonl(0xFFFF);
		u.dw[3] = htonl(ip);
		this->port = port;
	}
	void setIP(const Ip6Address& ip, uint16_t port)
	{
		memcpy(u.b, ip.data, 16);
		this->port = port;
	}
	void setIP(const IpAddress& ip, uint16_t port)
	{
		if (ip.type == AF_INET)
			setIP(ip.data.v4, port);
		else
			setIP(ip.data.v6, port);
	}
	void getIP(IpAddress& ip) const
	{
		if (u.dw[0] == 0 && u.dw[1] == 0 && u.dw[2] == htonl(0xFFFF))
		{
			ip.type = AF_INET;
			ip.data.v4 = ntohl(u.dw[3]);
		}
		else
		{
			ip.type = AF_INET6;
			memcpy(ip.data.v6.data, u.b, 16);
		}
	}
	uint32_t getHash() const
	{
		return u.dw[0] ^ u.dw[1] ^ u.dw[2] ^ u.dw[3] ^ port;
	}
};

namespace boost
{
	template<> struct hash<IpKey>
	{
		size_t operator()(const IpKey& x) const { return x.getHash(); }
	};

	template<> struct hash<IpPortKey>
	{
		size_t operator()(const IpPortKey& x) const { return x.getHash(); }
	};
}

#endif // IP_KEY_H_

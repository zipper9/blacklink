#ifndef RESOLVER_H_
#define RESOLVER_H_

#include "IpAddress.h"

namespace Resolver
{
		static const int RESOLVE_RESULT_V4 = 1;
		static const int RESOLVE_RESULT_V6 = 2;
		static const int RESOLVE_TYPE_EXACT = 1024;

		int resolveHost(Ip4Address* v4, Ip6AddressEx* v6, int af, const string& host, bool* isNumeric = nullptr) noexcept;
		bool resolveHost(IpAddressEx& addr, int type, const string& host, bool* isNumeric = nullptr) noexcept;
		string getHostName(const IpAddress& ip);
}

#endif // RESOLVER_H_

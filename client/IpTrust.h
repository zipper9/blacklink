#ifndef IP_TRUST_H_
#define IP_TRUST_H_

#include "IpList.h"
#include "RWLock.h"

class IpTrust
{
	public:
		IpTrust();
		bool isBlocked(uint32_t addr) const noexcept;
		void load() noexcept;
		void clear() noexcept;
		void updateSettings() noexcept;
		static std::string getFileName();

	private:
		IpList ipList;
		mutable unique_ptr<RWLock> cs;
		bool enabled;
		bool hasWhiteList;
};

extern IpTrust ipTrust;

#endif // IP_TRUST_H_

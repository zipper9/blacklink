#ifndef NETWORK_DEVICES_H_
#define NETWORK_DEVICES_H_

#include "IpAddress.h"
#include "Locks.h"

class NetworkDevices
{
public:
	NetworkDevices() noexcept;
	bool getDeviceAddress(int af, const string& name, IpAddressEx& addr, bool forceUpdate = false) noexcept;

private:
	typedef boost::unordered_map<string, IpAddressEx> DeviceAddrMap;

	DeviceAddrMap devAddr4;
	DeviceAddrMap devAddr6;
	uint64_t updateTime;
	CriticalSection cs;
};

extern NetworkDevices networkDevices;

#endif // NETWORK_DEVICES_H_

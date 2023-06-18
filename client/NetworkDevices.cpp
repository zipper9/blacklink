#include "stdinc.h"
#include "NetworkDevices.h"
#include "NetworkUtil.h"
#include "TimeUtil.h"

NetworkDevices networkDevices;

NetworkDevices::NetworkDevices() noexcept
{
	updateTime = 0;
}

bool NetworkDevices::getDeviceAddress(int af, const string& name, IpAddressEx& addr, bool forceUpdate) noexcept
{
	bool result = false;
	DeviceAddrMap& dam = af == AF_INET6 ? devAddr6 : devAddr4;
	vector<Util::AdapterInfo> adapters;
	uint64_t tick = GET_TICK();

	cs.lock();
	if (forceUpdate || tick >= updateTime)
	{
		updateTime = tick + 300 * 1000; // update every 5 minutes
		Util::getNetworkAdapters(af, adapters);
		dam.clear();
		for (const auto& item : adapters)
			dam.insert(make_pair(item.name, item.ip));
	}
	auto it = dam.find(name);
	if (it != dam.end())
	{
		addr = it->second;
		result = true;
	}
	cs.unlock();

	return result;
}

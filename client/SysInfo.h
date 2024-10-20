#ifndef SYS_INFO_H_
#define SYS_INFO_H_

#include "typedefs.h"

#ifdef _WIN32
#include "w.h"
#endif

namespace SysInfo
{
#ifdef _WIN32
	struct DiskInfo
	{
		tstring mountPath;
		int64_t size;
		int64_t free;
		int type;
	};

	void getDisks(std::vector<DiskInfo>& results);
	TStringList getVolumes();
	string getCPUInfo();
	bool getMemoryInfo(MEMORYSTATUSEX* status);
	uint64_t getTickCount();
	inline uint64_t getSysUptime() { return getTickCount() / 1000; }
#endif
}

#endif // SYS_INFO_H_

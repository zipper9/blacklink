#ifndef APP_STATS_H_
#define APP_STATS_H_

#include "typedefs.h"

namespace AppStats
{
	string getStats();
	string getNetworkStats();
	string getSpeedInfo();
	string getDiskInfo();
	string getDiskSpaceInfo(bool onlyTotal = false);
	string getGlobalMemoryStatusMessage();
	string getStartupMessage();
	string getFullSystemStatusMessage();
}

#endif // APP_STATS_H_

#ifndef SETTINGS_UTIL_H_
#define SETTINGS_UTIL_H_

#include "typedefs.h"

namespace Conf
{
	struct IPSettings
	{
		int autoDetect;
		int incomingConnections;
		int manualIp;
		int noIpOverride;
		int bindAddress;
		int bindDevice;
		int bindOptions;
		int externalIp;
		int mapper;
	};

	void getIPSettings(IPSettings& s, bool v6);
}

namespace Util
{
	void updateCoreSettings();
	std::string getConfString(int id);
	void setConfString(int id, const std::string& s);
	int getConfInt(int id);
	void setConfInt(int id, int value);
	void loadOtherSettings();
	bool loadLanguage();
	void getUserInfoHash(uint8_t out[]);
}

#endif // SETTINGS_UTIL_H_

#ifndef IP_INFO_H_
#define IP_INFO_H_

#include <string>

struct IPInfo
{
	enum
	{
		FLAG_COUNTRY   = 1,
		FLAG_LOCATION  = 2,
		FLAG_P2P_GUARD = 4
	};
	
	int known;
	std::string country;
	std::string location;
	std::string p2pGuard;
	int countryImage;
	int locationImage;

	void clearCountry()
	{
		countryImage = 0;
		country.clear();
	}

	void clearLocation()
	{
		locationImage = 0;
		location.clear();
	}

	IPInfo() : known(0), countryImage(0), locationImage(0) {}
};

#endif // IP_INFO_H_

#include "stdafx.h"
#include "KnownClients.h"

const KnownClients::Client KnownClients::clients[] =
{
	// Up from http://ru.wikipedia.org/wiki/DC%2B%2B
	{ "FlylinkDC++", A_SHORT_VERSIONSTRING "-" A_REVISION_NUM_STR },
	{ "++", DCVERSIONSTRING   }, // Version from core.
	{ "EiskaltDC++", "2.2.10" }, // Oct, 2019 https://github.com/eiskaltdcpp/eiskaltdcpp
	{ "AirDC++", "3.55"       }, // Oct, 2019 http://www.airdcpp.net/
	{ "RSX++", "1.21"         }, // 14 apr 2011 http://rsxplusplus.sourceforge.net/
	{ "ApexDC++", "1.6.5"     }, // 20 aug 2014 http://www.apexdc.net/changes/ (http://forums.apexdc.net/topic/4670-apexdc-160-available-for-download/)
	{ "PWDC++", "0.41"        }, // 29th Dec 2005: Project discontinued
	{ "IceDC++", "1.01a"      }, // 17 jul 2009 http://sourceforge.net/projects/icedc/
	{ "StrgDC++", "2.42"      }, // latest public beta (project possible dead) http://strongdc.sourceforge.net/download.php?lang=eng
	{ "Lama", "500"           }, // http://lamalama.tv
	{ nullptr, nullptr        }  // terminating, don't delete this
};

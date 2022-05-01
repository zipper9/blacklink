#include "stdafx.h"
#include "KnownClients.h"

const KnownClients::Client KnownClients::clients[] =
{
	// Up from http://ru.wikipedia.org/wiki/DC%2B%2B
	{ "FlylinkDC++", "r504-x64-22345" },
	{ "++", DCVERSIONSTRING   }, // Version from core.
	{ "EiskaltDC++", "2.4.2"  }, // Mar 03, 2021 https://github.com/eiskaltdcpp/eiskaltdcpp
	{ "AirDC++", "4.11"       }, // 20 September 2021 http://www.airdcpp.net/
	{ "RSX++", "1.21"         }, // 14 apr 2011 http://rsxplusplus.sourceforge.net/
	{ "ApexDC++", "1.6.5"     }, // 20 aug 2014 http://www.apexdc.net/changes/ (http://forums.apexdc.net/topic/4670-apexdc-160-available-for-download/)
	{ "PWDC++", "0.41"        }, // 29th Dec 2005: Project discontinued
	{ "IceDC++", "1.01a"      }, // 17 jul 2009 http://sourceforge.net/projects/icedc/
	{ "StrgDC++", "2.42"      }, // latest public beta (project possible dead) http://strongdc.sourceforge.net/download.php?lang=eng
	{ "gl++", "0.61"          }, // latest version, 2012
	{ "Lama", "500"           }, // http://lamalama.tv
	{ nullptr, nullptr        }  // terminating, don't delete this
};

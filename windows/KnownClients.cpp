#include "stdafx.h"
#include "KnownClients.h"
#include "version.h"

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

const char* KnownClients::userAgents[] =
{
	"FlylinkDC++ r504 build 22345",
	"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.111 Safari/537.36", // Chrome
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:104.0) Gecko/20100101 Firefox/104.0", // Firefox
	"Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)", // Google bot
	"Mozilla/5.0 (iPhone12,1; U; CPU iPhone OS 13_0 like Mac OS X) AppleWebKit/602.1.50 (KHTML, like Gecko) Version/10.0 Mobile/15E148 Safari/602.1", // iPhone 11
	"curl/7.64.1",
	"python-requests/2.27.1",
	nullptr
};

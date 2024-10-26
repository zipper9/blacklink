#include "stdinc.h"
#include "HubEntry.h"
#include "ClientManager.h"
#include "Util.h"

bool HubEntry::checkKeyPrintFormat(const string& kp)
{
	if (!(kp.length() == 52 + 7 && kp.compare(0, 7, "SHA256/", 7) == 0)) return false;
	uint8_t dst[32];
	bool error;
	Util::fromBase32(kp.c_str() + 7, dst, sizeof(dst), &error);
	return !error;
}

HubEntry::HubEntry(const string& name, const string& server, const string& description, const string& users, const string& country,
                   const string& shared, const string& minShare, const string& minSlots, const string& maxHubs, const string& maxUsers,
                   const string& reliability, const string& rating, const string& encoding,
                   const string& secureUrl, const string& website, const string& email,
                   const string& software, const string& network) :

                   name(name), description(description), country(country), rating(rating), reliability((float) Util::toDouble(reliability) / 100.0f),
                   shared(Util::toInt64(shared)), minShare(Util::toInt64(minShare)), users(Util::toInt(users)), maxUsers(Util::toInt(maxUsers)),
                   minSlots(Util::toInt(minSlots)), maxHubs(Util::toInt(maxHubs)), encoding(encoding), website(website), email(email),
                   software(software), network(network)
{
	Util::ParsedUrl p;
	if (!server.empty())
	{
		Util::decodeUrl(server, p);
		string kp = Util::getQueryParam(p.query, "kp");
		if (!kp.empty() && checkKeyPrintFormat(kp))
			keyPrint = std::move(kp);
		this->server = Util::formatDchubUrl(p);
	}
	if (!secureUrl.empty())
	{
		Util::decodeUrl(secureUrl, p);
		if (keyPrint.empty())
		{
			string kp = Util::getQueryParam(p.query, "kp");
			if (!kp.empty() && checkKeyPrintFormat(kp))
				keyPrint = std::move(kp);
		}
		this->secureUrl = Util::formatDchubUrl(p);
	}
}

string FavoriteHubEntry::getNick(bool useDefault) const
{
	if (!nick.empty() || !useDefault) return nick;
	return ClientManager::getDefaultNick();
}

double FavoriteHubEntry::parseSizeString(const string& s, double* origSize, int* unit)
{
	int u = 0;
	double result = Util::toDouble(s);
	if (origSize) *origSize = result;
	auto pos = s.find_first_of("KMGTkmgt");
	if (pos != string::npos)
	{
		switch (s[pos])
		{
			case 'K': case 'k': u = 1; break;
			case 'M': case 'm': u = 2; break;
			case 'G': case 'g': u = 3; break;
			case 'T': case 't': u = 4;
		}
		result *= 1ull<<(10*u);
	}
	if (unit) *unit = u;
	return result;
}

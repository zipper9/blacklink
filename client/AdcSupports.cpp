/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "stdinc.h"
#include "AdcSupports.h"
#include "OnlineUser.h"
#include "StringTokenizer.h"

#if defined(BL_FEATURE_COLLECT_UNKNOWN_FEATURES) || defined(BL_FEATURE_COLLECT_UNKNOWN_TAGS)
#include "TagCollector.h"
#endif

const string AdcSupports::CLIENT_PROTOCOL("ADC/1.0");
const string AdcSupports::SECURE_CLIENT_PROTOCOL_TEST("ADCS/0.10");
const string AdcSupports::ADCS_FEATURE("ADC0");
const string AdcSupports::TCP4_FEATURE("TCP4");
const string AdcSupports::TCP6_FEATURE("TCP6");
const string AdcSupports::UDP4_FEATURE("UDP4");
const string AdcSupports::UDP6_FEATURE("UDP6");
const string AdcSupports::NAT0_FEATURE("NAT0");
const string AdcSupports::SEGA_FEATURE("SEGA");
const string AdcSupports::CCPM_FEATURE("CCPM");
const string AdcSupports::BASE_SUPPORT("ADBASE");
const string AdcSupports::BAS0_SUPPORT("ADBAS0");
const string AdcSupports::TIGR_SUPPORT("ADTIGR");
const string AdcSupports::UCM0_SUPPORT("ADUCM0");
const string AdcSupports::BLO0_SUPPORT("ADBLO0");
const string AdcSupports::ZLIF_SUPPORT("ADZLIF");

#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
TagCollector collAdcFeatures;
TagCollector collNmdcFeatures;
#endif

#ifdef BL_FEATURE_COLLECT_UNKNOWN_TAGS
TagCollector collNmdcTags;
#endif

string AdcSupports::getSupports(const Identity& id)
{
	string tmp;
	const auto& u = id.getUser();
	const auto flags = u->getFlags();

#define CHECK_FEAT(feat) if (flags & User::feat) { tmp += feat##_FEATURE + ' '; }

#define CHECK_SUP(sup) if (flags & User::sup) { tmp += sup##_SUPPORT + ' '; }

	CHECK_FEAT(TCP4);
	CHECK_FEAT(UDP4);
	CHECK_FEAT(TCP6);
	CHECK_FEAT(UDP6);
	CHECK_FEAT(ADCS);
	CHECK_FEAT(NAT0);
	CHECK_FEAT(CCPM);

#undef CHECK_FEAT

#undef CHECK_SUP

	const auto su = id.getKnownSupports();

#define CHECK_FEAT(feat) if (su & feat##_FEATURE_BIT) { tmp += feat##_FEATURE + ' '; }

	CHECK_FEAT(SEGA);

#undef CHECK_FEAT

	return tmp;
}

void AdcSupports::setSupports(Identity& id, const StringList& su, const string& source, uint32_t* parsedFeatures)
{
	uint8_t knownSupports = 0;
	auto& u = id.getUser();
	User::MaskType flags = 0;

	for (auto i = su.cbegin(); i != su.cend(); ++i)
	{

#define CHECK_FEAT(feat) if (*i == feat##_FEATURE) { flags |= User::feat; }

#define CHECK_SUP_BIT(feat) if (*i == feat##_FEATURE) { knownSupports |= feat##_FEATURE_BIT; }

		CHECK_FEAT(TCP4) else
		CHECK_FEAT(UDP4) else
		CHECK_FEAT(TCP6) else
		CHECK_FEAT(UDP6) else
		CHECK_FEAT(ADCS) else
		CHECK_FEAT(NAT0) else
		CHECK_FEAT(CCPM) else
		CHECK_SUP_BIT(SEGA)

#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
		else
			collAdcFeatures.addTag(*i, source);
#endif

#undef CHECK_FEAT
#undef CHECK_SUP_BIT

	}
	u->setFlag(flags);
	id.setKnownSupports(knownSupports);
	if (parsedFeatures) *parsedFeatures = flags;
}

void AdcSupports::setSupports(Identity& id, const string& su, const string& source, uint32_t* parsedFeatures)
{
	setSupports(id, StringTokenizer<string>(su, ',').getWritableTokens(), source, parsedFeatures);
}

#if defined(BL_FEATURE_COLLECT_UNKNOWN_FEATURES) || defined(BL_FEATURE_COLLECT_UNKNOWN_TAGS)
string AdcSupports::getCollectedUnknownTags()
{
	string result;
#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
	string s1 = collAdcFeatures.getInfo();
	if (!s1.empty())
	{
		result += "ADC features:\n";
		result += s1;
	}
	s1 = collNmdcFeatures.getInfo();
	if (!s1.empty())
	{
		result += "NMDC features:\n";
		result += s1;
	}
#endif
#ifdef BL_FEATURE_COLLECT_UNKNOWN_TAGS
	string s2 = collNmdcTags.getInfo();
	if (!s2.empty())
	{
		result += "NMDC tags:\n";
		result += s2;
	}
#endif
	return result;
}
#endif

void NmdcSupports::setStatus(Identity& id, const char statusChar, const char modeChar, const string& connection)
{
	if (!connection.empty())
	{
		auto con = Util::toDouble(connection);
		double coef;
		const auto i = connection.find_first_not_of("1234567890 ,."); // TODO - отложить парсинг соединения позже
		if (i == string::npos)
		{
			coef = 1000 * 1000 / 8; // no postfix - megabits.
		}
		else
		{
			auto postfix = connection.substr(i);

#define CHECK_SPEED(s, c) if (postfix == s) { coef = c; }

			CHECK_SPEED("KiB/s", 1024)
			/*
			else CHECK_SPEED("MiB/s", 1024 * 1024)
			else CHECK_SPEED("B/s", 1)
			*/
			// http://nmdc.sourceforge.net/NMDC.html#_myinfo
			// Connection is a string for the connection:
			// Default NMDC1 connections types 28.8Kbps, 33.6Kbps, 56Kbps, Satellite, ISDN, DSL, Cable, LAN(T1), LAN(T3)
			// Default NMDC2 connections types Modem, DSL, Cable, Satellite, LAN(T1), LAN(T3)
			else CHECK_SPEED("Kbps", 1000)
				else
				{
					coef = 0;
#ifdef FLYLINKDC_COLLECT_UNKNOWN_FEATURES
					// LOCK(g_debugCsUnknownNmdcConnection);
					// g_debugUnknownNmdcConnection.insert(postfix);
#endif
				}
#undef CHECK_SPEED
		}
		con *= coef;
		id.setDownloadSpeed(static_cast<uint32_t>(con));
	}

	const auto& u = id.getUser();
	User::MaskType setUserFlags = 0;
	User::MaskType unsetUserFlags = 0;
	uint8_t statusFlags = 0;
	uint8_t statusMask = Identity::SF_AWAY | Identity::SF_SERVER | Identity::SF_FIREBALL;

	if (statusChar & AWAY)
		statusFlags |= Identity::SF_AWAY;
	if (statusChar & SERVER)
		statusFlags |= Identity::SF_SERVER;
	if (statusChar & FIREBALL)
		statusFlags |= Identity::SF_FIREBALL;
	if (statusChar & TLS)
		setUserFlags |= User::TLS;
	else
		unsetUserFlags |= User::TLS;
	if (statusChar & NAT0)
		setUserFlags |= User::NAT0;
	else
		unsetUserFlags |= User::NAT0;
	if (modeChar == 'A')
	{
		unsetUserFlags |= User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE;
		statusMask |= Identity::SF_PASSIVE;
	}
	else if (modeChar == 'P' || modeChar == '5')
	{
		statusFlags |= Identity::SF_PASSIVE;
		setUserFlags |= User::NMDC_FILES_PASSIVE | User::NMDC_SEARCH_PASSIVE;
		statusMask |= Identity::SF_PASSIVE;
	}
	id.setStatusBits(statusFlags, statusMask);
	u->changeFlags(setUserFlags, unsetUserFlags);
}

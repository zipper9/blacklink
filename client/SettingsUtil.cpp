#include "stdinc.h"
#include "SettingsUtil.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include "DatabaseManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "SearchManager.h"
#include "FinishedManager.h"
#include "ThrottleManager.h"
#include "ShareManager.h"
#include "FavoriteManager.h"
#include "CryptoManager.h"
#include "IpGuard.h"
#include "IpTrust.h"
#include "SimpleXML.h"
#include "AppPaths.h"
#include "ChatOptions.h"
#include "ConfCore.h"

void Conf::getIPSettings(IPSettings& s, bool v6)
{
	if (v6)
	{
		s.autoDetect = AUTO_DETECT_CONNECTION6;
		s.manualIp = WAN_IP_MANUAL6;
		s.noIpOverride = NO_IP_OVERRIDE6;
		s.incomingConnections = INCOMING_CONNECTIONS6;
		s.bindAddress = BIND_ADDRESS6;
		s.bindDevice = BIND_DEVICE6;
		s.bindOptions = BIND_OPTIONS6;
		s.externalIp = EXTERNAL_IP6;
		s.mapper = MAPPER6;
	}
	else
	{
		s.autoDetect = AUTO_DETECT_CONNECTION;
		s.manualIp = WAN_IP_MANUAL;
		s.noIpOverride = NO_IP_OVERRIDE;
		s.incomingConnections = INCOMING_CONNECTIONS;
		s.bindAddress = BIND_ADDRESS;
		s.bindDevice = BIND_DEVICE;
		s.bindOptions = BIND_OPTIONS;
		s.externalIp = EXTERNAL_IP;
		s.mapper = MAPPER;
	}
}

void Util::updateCoreSettings()
{
	LogManager::updateSettings();
	ChatOptions::updateSettings();
	DatabaseManager::getInstance()->updateSettings();
	DownloadManager::getInstance()->updateSettings();
	UploadManager::getInstance()->updateSettings();
	SearchManager::getInstance()->updateSettings();
	FinishedManager::getInstance()->updateSettings();
	ThrottleManager::getInstance()->updateSettings();
	CryptoManager::getInstance()->updateSettings();
	ipGuard.updateSettings();
	ipTrust.updateSettings();
}

string Util::getConfString(int id)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	string result = ss->getString(id);
	ss->unlockRead();
	return result;
}

void Util::setConfString(int id, const string& s)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	ss->setString(id, s);
	ss->unlockWrite();
}

int Util::getConfInt(int id)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	int result = ss->getInt(id);
	ss->unlockRead();
	return result;
}

void Util::setConfInt(int id, int value)
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	ss->setInt(id, value);
	ss->unlockWrite();
}

void Util::loadOtherSettings()
{
	try
	{
		SimpleXML xml;
		xml.fromXML(File(SettingsManager::getConfigFile(), File::READ, File::OPEN).read());
		xml.stepIn();
		ShareManager::getInstance()->load(xml);
		auto fm = FavoriteManager::getInstance();
		fm->loadLegacyRecents(xml);
		fm->loadPreviewApps(xml);
		fm->loadSearchUrls(xml);

		xml.stepOut();
	}
	catch (const FileException&)
	{
	}
	catch (const SimpleXMLException&)
	{
	}
}

bool Util::loadLanguage()
{
	auto path = Util::getLanguagesPath();
	auto name = getConfString(Conf::LANGUAGE_FILE);
	if (name.empty())
	{
		// TODO: Determine language from user's locale
		name = "en-US.xml";
	}
	path += name;
	return ResourceManager::loadLanguage(path);
}

static void updateString(TigerHash& tiger, const string& s)
{
	uint32_t len = s.length();
	tiger.update(&len, sizeof(len));
	tiger.update(s.data(), len);
}

void Util::getUserInfoHash(uint8_t out[])
{
	TigerHash tiger;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	updateString(tiger, ss->getString(Conf::NICK));
	updateString(tiger, ss->getString(Conf::EMAIL));
	updateString(tiger, ss->getString(Conf::DESCRIPTION));
	updateString(tiger, ss->getString(Conf::UPLOAD_SPEED));
	uint32_t val = ss->getInt(Conf::GENDER);
	tiger.update(&val, sizeof(val));
	ss->unlockRead();
	memcpy(out, tiger.finalize(), TigerHash::BYTES);
}

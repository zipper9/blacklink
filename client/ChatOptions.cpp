#include "stdinc.h"
#include "ChatOptions.h"
#include "SettingsManager.h"
#include "ConfCore.h"

static std::atomic_int chatOptions = 0;

int ChatOptions::getOptions()
{
	return chatOptions;
}

void ChatOptions::updateSettings()
{
	int newOptions = 0;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	if (ss->getBool(Conf::IP_IN_CHAT))
		newOptions |= OPTION_SHOW_IP;
	if (ss->getBool(Conf::COUNTRY_IN_CHAT))
		newOptions |= OPTION_SHOW_COUNTRY;
	if (ss->getBool(Conf::ISP_IN_CHAT))
		newOptions |= OPTION_SHOW_ISP;
	if (ss->getBool(Conf::SEND_SLOTGRANT_MSG))
		newOptions |= OPTION_SEND_GRANT_MSG;
	if (ss->getBool(Conf::SUPPRESS_MAIN_CHAT))
		newOptions |= OPTION_SUPPRESS_MAIN_CHAT;
	if (ss->getBool(Conf::SUPPRESS_PMS))
		newOptions |= OPTION_SUPPRESS_PM;
	if (ss->getBool(Conf::IGNORE_ME))
		newOptions |= OPTION_IGNORE_ME;
	if (ss->getBool(Conf::IGNORE_HUB_PMS))
		newOptions |= OPTION_IGNORE_HUB_PMS;
	if (ss->getBool(Conf::IGNORE_BOT_PMS))
		newOptions |= OPTION_IGNORE_BOT_PMS;
	if (ss->getBool(Conf::PROTECT_PRIVATE))
		newOptions |= OPTION_PROTECT_PRIVATE;
	if (ss->getBool(Conf::LOG_PRIVATE_CHAT))
		newOptions |= OPTION_LOG_PRIVATE_CHAT;
	if (ss->getBool(Conf::LOG_IF_SUPPRESS_PMS))
		newOptions |= OPTION_LOG_SUPPRESSED;
	if (ss->getBool(Conf::LOG_MAIN_CHAT))
		newOptions |= OPTION_LOG_MAIN_CHAT;
	if (ss->getBool(Conf::FILTER_MESSAGES))
		newOptions |= OPTION_FILTER_KICK;
	ss->unlockRead();
	chatOptions.store(newOptions);
}

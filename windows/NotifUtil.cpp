#include <stdafx.h>
#include "NotifUtil.h"
#include "PopupManager.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"

bool NotifUtil::isPopupEnabled(int setting)
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	return !ss->getBool(Conf::POPUPS_DISABLED) && ss->getBool(setting);
}

void NotifUtil::showPopup(int setting, const tstring& msg, const tstring& title, int flags)
{
	if (isPopupEnabled(setting))
		PopupManager::getInstance()->show(msg, title, flags);
}

void NotifUtil::showPopup(int setting, const tstring& msg, const tstring& extMsg, size_t extLen, const tstring& title)
{
	if (!isPopupEnabled(setting)) return;
	tstring text = msg;
	text += _T(": ");
	if (extMsg.length() <= extLen)
		text += extMsg;
	else
		text += extMsg.substr(0, extLen);
	PopupManager::getInstance()->show(text, title, NIIF_INFO);
}

void NotifUtil::playSound(int setting)
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::SOUNDS_DISABLED)) return;
	WinUtil::playSound(ss->getString(setting), false);
}

void NotifUtil::playBeep(int setting)
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::SOUNDS_DISABLED) || !ss->getBool(setting)) return;
	WinUtil::playSound(ss->getString(Conf::SOUND_BEEPFILE), true);
}

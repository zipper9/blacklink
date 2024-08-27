#ifndef NOTIF_UTIL_H_
#define NOTIF_UTIL_H_

#include "../client/typedefs.h"

#define SHOW_POPUP(setting, msg, title) \
	NotifUtil::showPopup(Conf::setting, msg, title, NIIF_INFO)

#define SHOW_POPUPF(setting, msg, title, flags) \
	NotifUtil::showPopup(Conf::setting, msg, title, flags)

#define SHOW_POPUP_EXT(setting, msg, extMsg, extLen, title) \
	NotifUtil::showPopup(Conf::setting, msg, extMsg, extLen, title)

#define PLAY_SOUND(setting) \
	NotifUtil::playSound(Conf::setting)

#define PLAY_SOUND_BEEP(setting) \
	NotifUtil::playBeep(Conf::setting)

namespace NotifUtil
{
	bool isPopupEnabled(int setting);
	void showPopup(int setting, const tstring& msg, const tstring& title, int flags);
	void showPopup(int setting, const tstring& msg, const tstring& extMsg, size_t extLen, const tstring& title);
	void playSound(int setting);
	void playBeep(int setting);
}

#endif // NOTIF_UTIL_H_

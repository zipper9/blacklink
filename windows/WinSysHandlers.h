#ifndef WIN_SYS_HANDLERS_H_
#define WIN_SYS_HANDLERS_H_

namespace WinUtil
{
	enum
	{
		REG_HANDLER_HUB_URL = 1,
		REG_HANDLER_MAGNET  = 2,
		REG_HANDLER_DCLST   = 4
	};

	extern int registeredHandlerMask;

	void registerHubUrlHandlers();
	void registerMagnetHandler();
	void registerDclstHandler();
	void unregisterHubUrlHandlers();
	void unregisterMagnetHandler();
	void unregisterDclstHandler();

	int getRegHandlerSettings();
	void applyRegHandlerSettings(int settings, int mask);
}

#endif // WIN_SYS_HANDLERS_H_

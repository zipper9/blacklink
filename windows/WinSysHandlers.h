#ifndef WIN_SYS_HANDLERS_H_
#define WIN_SYS_HANDLERS_H_

namespace WinUtil
{
	extern bool hubUrlHandlersRegistered;
	extern bool magnetHandlerRegistered;
	extern bool dclstHandlerRegistered;

	void registerHubUrlHandlers();
	void registerMagnetHandler();
	void registerDclstHandler();
	void unregisterHubUrlHandlers();
	void unregisterMagnetHandler();
	void unregisterDclstHandler();
}

#endif // WIN_SYS_HANDLERS_H_

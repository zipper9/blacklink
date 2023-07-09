#ifndef MENU_HELPER_H_
#define MENU_HELPER_H_

#include "OMenu.h"

namespace MenuHelper
{
	extern CMenu mainMenu;
	extern OMenu copyHubMenu;

	void createMenus();
	void uninit();
	void appendPrioItems(OMenu& menu, int idFirst);
	void addStaticMenu(HMENU hMenu);
	void removeStaticMenu(HMENU hMenu);
	void unlinkStaticMenus(OMenu &menu);
}

#endif // MENU_HELPER_H_

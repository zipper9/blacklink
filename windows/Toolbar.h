#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include "../client/ResourceManager.h"

struct ToolbarButton
{
	int id;
	bool check;
	ResourceManager::Strings tooltip;
};

struct MenuImage
{
	int id;
	int image;
};

extern const ToolbarButton g_ToolbarButtons[];
extern const ToolbarButton g_WinampToolbarButtons[];
extern const MenuImage g_MenuImages[];

extern const int g_ToolbarButtonsCount;
extern const int g_WinampToolbarButtonsCount;

void fillToolbarButtons(CToolBarCtrl& toolbar, const string& setting, const ToolbarButton* buttons, int buttonCount, const uint8_t* checkState);

#endif /* TOOLBAR_H */

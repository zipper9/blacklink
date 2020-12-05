#ifndef TOOLBAR_H
#define TOOLBAR_H

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

#endif /* TOOLBAR_H */

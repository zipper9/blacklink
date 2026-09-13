#ifndef HUB_WINDOW_INFO_H_
#define HUB_WINDOW_INFO_H_

#include <string>

struct HubWindowInfo
{
	int windowPosX;
	int windowPosY;
	int windowSizeX;
	int windowSizeY;
	int windowType;
	bool hideUserList;
	std::string headerOrder;
	std::string headerWidths;
	std::string headerVisible;
	int headerSort;
	bool headerSortAsc;
	int chatUserSplit;
	bool swapPanels;
};

#endif // HUB_WINDOW_INFO_H_

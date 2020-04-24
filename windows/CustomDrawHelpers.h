#ifndef CUSTOM_DRAW_HELPERS_H
#define CUSTOM_DRAW_HELPERS_H

#include "../client/LocationUtil.h"
#include <atlctrls.h>

class BaseImageList;

namespace CustomDrawHelpers
{

void drawLocation(CListViewCtrl& lv, bool lvFocused, const NMLVCUSTOMDRAW* cd, const Util::CustomNetworkIndex& cni);
void drawTextAndIcon(CListViewCtrl& lv, bool lvFocused, const NMLVCUSTOMDRAW* cd, BaseImageList& images, int icon, const tstring& text);
void drawIPAddress(CListViewCtrl& lv, bool lvFocused, const NMLVCUSTOMDRAW* cd, bool isPhantomIP, const tstring& text);

bool drawVideoResIcon(CListViewCtrl& lv, bool lvFocused, const LPNMLVCUSTOMDRAW cd, const tstring& text, unsigned width, unsigned height);
bool parseVideoResString(const tstring& text, unsigned& width, unsigned& height);

}

#endif /* CUSTOM_DRAW_HELPERS_H */

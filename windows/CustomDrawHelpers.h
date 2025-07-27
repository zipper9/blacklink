#ifndef CUSTOM_DRAW_HELPERS_H
#define CUSTOM_DRAW_HELPERS_H

#include "../client/typedefs.h"
#include "../client/IPInfo.h"
#include <atlapp.h>
#include <atlwin.h>

namespace CustomDrawHelpers
{
	enum
	{
		FLAG_APP_THEMED      = 0x01,
		FLAG_USE_HOT_ITEM    = 0x02,
		FLAG_GET_COLFMT      = 0x04,
		FLAG_LV_FOCUSED      = 0x08,
		FLAG_SELECTED        = 0x10,
		FLAG_FOCUSED         = 0x20,
		FLAG_HOT             = 0x40,
		FLAG_HIDE_FOCUS_RECT = 0x80
	};

	struct CustomDrawState
	{
		unsigned flags;
		int currentItem;
		RECT rcItem;
		RECT rc;
		int oldBkMode;
		COLORREF oldColor;
		COLORREF clrForeground;
		COLORREF clrBackground;
		int drawCount;
		HIMAGELIST hImg;
		HIMAGELIST hImgState;
		int indent;
		int iconSpace;
		vector<int> columnFormat;

		CustomDrawState();
	};

	struct CustomDrawCheckBoxState
	{
		bool enableCustomDraw;
		bool initialized;
		COLORREF textColor;
		SIZE checkBoxSize;
		int checkBoxGap;

		CustomDrawCheckBoxState();
		void init(HWND hWnd, HDC dc);
	};

	void startDraw(CustomDrawState& state, const NMLVCUSTOMDRAW* cd);
	void startItemDraw(CustomDrawState& state, const NMLVCUSTOMDRAW* cd);
	void drawFocusRect(CustomDrawState& state, const NMLVCUSTOMDRAW* cd);
	void drawBackground(HTHEME hTheme, const CustomDrawState& state, const NMLVCUSTOMDRAW* cd);
	bool startSubItemDraw(CustomDrawState& state, const NMLVCUSTOMDRAW* cd);
	void endSubItemDraw(CustomDrawState& state, const NMLVCUSTOMDRAW* cd);
	void setColor(CustomDrawState& state, const NMLVCUSTOMDRAW* cd);
	void fillBackground(const CustomDrawState& state, const NMLVCUSTOMDRAW* cd);
	void drawFirstSubItem(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, const tstring& text);
	UINT getTextFlags(const CustomDrawState& state, const NMLVCUSTOMDRAW* cd);

	void drawLocation(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, const IPInfo& ipInfo);
	void drawCountry(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, uint16_t code, const tstring& text);
	void drawTextAndIcon(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, CImageList* images, int icon, const tstring& text, bool rightIcon);
	void drawIPAddress(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, bool isPhantomIP, const tstring& text);

	void drawVideoResIcon(CustomDrawState& state, const NMLVCUSTOMDRAW* cd, const tstring& text, unsigned width, unsigned height);
	bool parseVideoResString(const tstring& text, unsigned& width, unsigned& height);

	void measureComboBox(MEASUREITEMSTRUCT* mis, HFONT hFont);
	void drawComboBox(HWND hWnd, const DRAWITEMSTRUCT* dis, HIMAGELIST hImgList);

	bool drawCheckBox(const NMCUSTOMDRAW* cd, CustomDrawCheckBoxState& state);
}

#endif /* CUSTOM_DRAW_HELPERS_H */

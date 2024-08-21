#ifndef OTHER_COLORS_TAB_H_
#define OTHER_COLORS_TAB_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "SettingsStore.h"
#include "PropPageCallback.h"
#include "../client/BaseSettingsImpl.h"

class OtherColorsTab : public CDialogImpl<OtherColorsTab>
{
	public:
		enum { IDD = IDD_OTHER_COLORS_TAB };

		OtherColorsTab() : hBrush(nullptr), hFont(nullptr), selColor(nullptr), callback(nullptr) {}
		~OtherColorsTab() { cleanup(); }

		BEGIN_MSG_MAP(OtherColorsTab)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_DRAWITEM, onDrawItem)
		COMMAND_HANDLER(IDC_CHANGE_COLOR, BN_CLICKED, onChooseColor)
		COMMAND_HANDLER(IDC_SET_DEFAULT, BN_CLICKED, onSetDefaultColor)
		COMMAND_HANDLER(IDC_TABCOLOR_LIST, LBN_SELCHANGE, onTabListChange)
		COMMAND_HANDLER(IDC_SETTINGS_ODC_MENUBAR_LEFT, BN_CLICKED, onChooseMenuColor)
		COMMAND_HANDLER(IDC_SETTINGS_ODC_MENUBAR_RIGHT, BN_CLICKED, onChooseMenuColor)
		COMMAND_HANDLER(IDC_USE_CUSTOM_MENU, BN_CLICKED, onMenuOption)
		COMMAND_HANDLER(IDC_SETTINGS_ODC_MENUBAR_USETWO, BN_CLICKED, onMenuOption)
		COMMAND_HANDLER(IDC_SETTINGS_ODC_MENUBAR_BUMPED, BN_CLICKED, onMenuOption)
		COMMAND_HANDLER(IDC_MENU_SET_DEFAULT, BN_CLICKED, onMenuDefaults)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onChooseColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSetDefaultColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTabListChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			showCurrentColor();
			return 0;
		}
		LRESULT onMenuOption(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChooseMenuColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMenuDefaults(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			cleanup();
			return 0;
		}

		void loadSettings(const BaseSettingsImpl* ss);
		void saveSettings(BaseSettingsImpl* ss) const;
		void getValues(SettingsStore& ss) const;
		void setValues(const SettingsStore& ss);
		void updateTheme();
		void setColor(int index, COLORREF clr);
		void setCallback(PropPageCallback* p) { callback = p; }

		bool useCustomMenu;
		bool menuTwoColors;
		bool menuBumped;

	private:
		CListBox ctrlTabList;
		CStatic ctrlSample;
		COLORREF currentColor;
		HBRUSH hBrush;
		HFONT hFont;

		COLORREF menuLeftColor;
		COLORREF menuRightColor;
		COLORREF* selColor;
		CStatic ctrlMenubar;
		PropPageCallback* callback;

		void showCurrentColor();
		void setCurrentColor(COLORREF cr);
		void updateMenuOptions();
		void cleanup();
		static UINT_PTR CALLBACK hookProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif // OTHER_COLORS_TAB_H_

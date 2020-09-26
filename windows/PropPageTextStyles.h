#ifndef _PROP_PAGE_TEXT_STYLES_H_
#define _PROP_PAGE_TEXT_STYLES_H_

#include <atlcrack.h>
#include "PropPage.h"
#include "../client/ConnectionManager.h"
#include "ChatCtrl.h"
#include "../client/SettingsManager.h"

class PropPageTextStyles: public CPropertyPage<IDD_TEXT_STYLES_PAGE>, public PropPage
{
	public:
		PropPageTextStyles();
		~PropPageTextStyles();
		
		BEGIN_MSG_MAP_EX(PropPageTextStyles)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, onExport)
		COMMAND_HANDLER(IDC_BACK_COLOR, BN_CLICKED, onEditBackColor)
		COMMAND_HANDLER(IDC_TEXT_COLOR, BN_CLICKED, onEditForeColor)
		COMMAND_HANDLER(IDC_TEXT_STYLE, BN_CLICKED, onEditTextStyle)
		COMMAND_HANDLER(IDC_DEFAULT_STYLES, BN_CLICKED, onDefaultStyles)
		
		COMMAND_HANDLER(IDC_TABCOLOR_LIST, LBN_SELCHANGE, onTabListChange)
		COMMAND_HANDLER(IDC_RESET_TAB_COLOR, BN_CLICKED, onSetDefaultColor)
		COMMAND_HANDLER(IDC_SELECT_TAB_COLOR, BN_CLICKED, onClientSelectTabColor)
		COMMAND_HANDLER(IDC_THEME_COMBO2, CBN_SELCHANGE, onImport)
		COMMAND_ID_HANDLER(IDC_BAN_COLOR, onSelectColor)
		
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCtlColor(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onExport(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditBackColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditForeColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditTextStyle(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onDefaultStyles(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onTabListChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSetDefaultColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClientSelectTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSelectColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+]NSL
		
		void setForeColor(CEdit& cs, COLORREF cr);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_COLORS; }
		void write();
		void cancel();

	private:
		class TextStyleSettings: public CHARFORMAT2
		{
			public:
				TextStyleSettings() : parent(nullptr) { }
				~TextStyleSettings() { }
				
				void init(PropPageTextStyles *parent, ResourceManager::Strings name,
				          SettingsManager::IntSetting bgSetting, SettingsManager::IntSetting fgSetting,
				          SettingsManager::IntSetting boldSetting, SettingsManager::IntSetting italicSetting);
				void loadSettings();
				void loadDefaultSettings();
				void saveSettings() const;
				void restoreSettings() const;
				void editBackgroundColor();
				void editForegroundColor();
				bool editTextStyle();
				
				ResourceManager::Strings name;
				
				PropPageTextStyles *parent;
				SettingsManager::IntSetting bgSetting;
				SettingsManager::IntSetting fgSetting;
				SettingsManager::IntSetting boldSetting;
				SettingsManager::IntSetting italicSetting;
				COLORREF savedTextColor, savedBackColor;
				DWORD savedEffects;
		};
		
	protected:
		enum
		{
			TS_GENERAL, TS_MYNICK, TS_MYMSG, TS_PRIVATE, TS_SYSTEM, TS_SERVER, TS_TIMESTAMP, TS_URL, TS_FAVORITE, TS_FAV_ENEMY, TS_OP,
			TS_LAST
		};
		
		struct ColorSettings
		{
			ResourceManager::Strings name;
			int setting;
			COLORREF value;
			COLORREF savedValue;
		};
		
		static ColorSettings colors[];		
		
		TextStyleSettings textStyles[TS_LAST];
		CListBox lsbList;
		ChatCtrl preview;
		LOGFONT mainFont;
		HFONT hMainFont;
		HBRUSH hBrush;
		COLORREF fg, bg;
		
		CListBox ctrlTabList;
		
		CButton cmdResetTab;
		CButton cmdSetTabColor;
		CEdit ctrlTabExample;
		
		bool firstLoad;
		tstring savedFont;
		COLORREF savedColors[2];

		bool defaultThemeSelected;
		tstring tempfile;

		struct ThemeInfo
		{
			tstring name;
			tstring path;
		};

		CComboBox ctrlTheme;
		vector<ThemeInfo> themes;

		void loadSettings();
		void saveSettings() const;
		void restoreSettings();
		void refreshPreview();
		void getThemeList();
};

#endif // _PROP_PAGE_TEXT_STYLES_H_

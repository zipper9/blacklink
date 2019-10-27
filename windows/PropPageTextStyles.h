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
		explicit PropPageTextStyles() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TEXT_STYLES))
		{
			fg = 0;
			bg = 0;
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
			mainColorChanged = false;
		}
		
		BEGIN_MSG_MAP_EX(PropPageTextStyles)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, onExport)
		COMMAND_HANDLER(IDC_BACK_COLOR, BN_CLICKED, onEditBackColor)
		COMMAND_HANDLER(IDC_TEXT_COLOR, BN_CLICKED, onEditForeColor)
		COMMAND_HANDLER(IDC_TEXT_STYLE, BN_CLICKED, onEditTextStyle)
		COMMAND_HANDLER(IDC_DEFAULT_STYLES, BN_CLICKED, onDefaultStyles)
		COMMAND_HANDLER(IDC_BLACK_AND_WHITE, BN_CLICKED, onBlackAndWhite)
		//COMMAND_HANDLER(IDC_SELWINCOLOR, BN_CLICKED, onEditBackground)
		//COMMAND_HANDLER(IDC_ERROR_COLOR, BN_CLICKED, onEditError)
		//COMMAND_HANDLER(IDC_ALTERNATE_COLOR, BN_CLICKED, onEditAlternate)
		COMMAND_ID_HANDLER(IDC_SELTEXT, onClickedText)
		
		COMMAND_HANDLER(IDC_TABCOLOR_LIST, LBN_SELCHANGE, onTabListChange)
		COMMAND_HANDLER(IDC_RESET_TAB_COLOR, BN_CLICKED, onClickedResetTabColor)
		COMMAND_HANDLER(IDC_SELECT_TAB_COLOR, BN_CLICKED, onClientSelectTabColor)
		COMMAND_HANDLER(IDC_THEME_COMBO2, CBN_SELCHANGE, onImport)
		COMMAND_ID_HANDLER(IDC_BAN_COLOR, onSelectColor)
		COMMAND_ID_HANDLER(IDC_DUPE_COLOR, onSelectColor)
		
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCtlColor(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onExport(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditBackColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditForeColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEditTextStyle(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onDefaultStyles(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onBlackAndWhite(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onClickedText(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		//LRESULT onEditBackground(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		//LRESULT onEditError(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		//LRESULT onEditAlternate(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onTabListChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedResetTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClientSelectTabColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSelectColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+]NSL
		
		void onResetColor(int i);
		
		void setForeColor(CEdit& cs, const COLORREF& cr)
		{
			HBRUSH hBr = CreateSolidBrush(cr);
			SetProp(cs.m_hWnd, _T("fillcolor"), hBr);
			cs.Invalidate();
			cs.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASENOW | RDW_UPDATENOW | RDW_FRAME);
		}
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_COLORS; }
		void write();
		void cancel();

	private:
		void RefreshPreview();
		
		class TextStyleSettings: public CHARFORMAT2
		{
			public:
				TextStyleSettings() : parent(nullptr) { }
				~TextStyleSettings() { }
				
				void Init(PropPageTextStyles *parent,
				          const char *text, const char *preview,
				          SettingsManager::IntSetting bgSetting, SettingsManager::IntSetting fgSetting,
				          SettingsManager::IntSetting boldSetting, SettingsManager::IntSetting italicSetting);
				void LoadSettings();
				void SaveSettings();
				void EditBackColor();
				void EditForeColor();
				void EditTextStyle();
				
				string text;
				string preview;
				
				PropPageTextStyles *parent;
				SettingsManager::IntSetting bgSetting;
				SettingsManager::IntSetting fgSetting;
				SettingsManager::IntSetting boldSetting;
				SettingsManager::IntSetting italicSetting;
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
		};
		
		static ColorSettings colors[];		
		
		TextStyleSettings textStyles[TS_LAST];
		CListBox lsbList;
		ChatCtrl preview;
		LOGFONT font;
		COLORREF fg, bg;
		
		CListBox ctrlTabList;
		
		CButton cmdResetTab;
		CButton cmdSetTabColor;
		CEdit ctrlTabExample;
		
		bool mainColorChanged;
		tstring tempfile;

		typedef boost::unordered_map<wstring, string> ColorThemeMap;
		typedef pair<wstring, string> ThemePair;
		CComboBox ctrlTheme;
		ColorThemeMap themeList;
		void getThemeList();
};

#endif // _PROP_PAGE_TEXT_STYLES_H_

#ifndef STYLES_PAGE_H_
#define STYLES_PAGE_H_

#include "PropPage.h"
#include "ChatStylesTab.h"
#include "UserListColorsTab.h"
#include "ProgressBarTab.h"
#include "OtherColorsTab.h"

class StylesPage : public CPropertyPage<IDD_TEXT_STYLES_PAGE>, public PropPage, public PropPageCallback
{
	public:
		StylesPage();

		BEGIN_MSG_MAP_EX(StylesPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, onChangeTab)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, onExport)
		COMMAND_HANDLER(IDC_THEME_COMBO, CBN_SELCHANGE, onSelectTheme)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onChangeTab(int idCtrl, LPNMHDR pnmh, BOOL& bHandled) { changeTab(); return TRUE; }
		LRESULT onSelectTheme(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onExport(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_COLORS; }
		void write();

		static bool queryChatColorsChanged();

	private:
		tstring currentTheme;
		bool themeModified;

		struct ThemeInfo
		{
			tstring name;
			tstring path;
			bool temp;
		};

		CComboBox ctrlTheme;
		vector<ThemeInfo> themes;

		CTabCtrl ctrlTabs;
		ChatStylesTab tabChat;
		UserListColorsTab tabUserList;
		ProgressBarTab tabProgress;
		OtherColorsTab tabOther;

		void getThemeList();
		bool saveTheme();
		void setThemeModified();
		void changeTab();

		void settingChanged(int id) override;
		void intSettingChanged(int id, int value) override;
};

#endif // STYLES_PAGE_H_

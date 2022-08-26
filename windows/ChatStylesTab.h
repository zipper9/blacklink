#ifndef CHAT_STYLES_TAB_H_
#define CHAT_STYLES_TAB_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "ChatCtrl.h"
#include "SettingsStore.h"
#include "PropPageCallback.h"
#include "../client/SettingsManager.h"

class FontStyleDlg : public CDialogImpl<FontStyleDlg>
{
	public:
		enum { IDD = IDD_FONT_STYLE };

		FontStyleDlg() : bold(false), italic(false) {}

		BEGIN_MSG_MAP_EX(FontStyleDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

		bool bold;
		bool italic;
};

class ChatStylesTab : public CDialogImpl<ChatStylesTab>
{
	public:
		enum { IDD = IDD_CHAT_COLORS_TAB };

		ChatStylesTab();
		~ChatStylesTab() { cleanup(); }

		BEGIN_MSG_MAP_EX(ChatStylesTab)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		COMMAND_HANDLER(IDC_CHOOSE_FONT, BN_CLICKED, onSetFont)
		COMMAND_HANDLER(IDC_SET_DEFAULT_FONT, BN_CLICKED, onSetDefaultFont)
		COMMAND_HANDLER(IDC_BACK_COLOR, BN_CLICKED, onSetBackgroundColor)
		COMMAND_HANDLER(IDC_TEXT_COLOR, BN_CLICKED, onSetTextColor)
		COMMAND_HANDLER(IDC_TEXT_STYLE, BN_CLICKED, onSetStyle)
		COMMAND_HANDLER(IDC_SET_DEFAULT, BN_CLICKED, onSetDefault)
		COMMAND_HANDLER(IDC_BOLD_MSG_AUTHORS, BN_CLICKED, onBoldMsgAuthor);
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSetFont(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onSetDefaultFont(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onSetStyle(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onSetTextColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onSetBackgroundColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onSetDefault(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onBoldMsgAuthor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			preview.Detach();
			cleanup();
			return 0;
		}

		void loadSettings();
		void saveSettings() const;
		void getValues(SettingsStore& ss) const;
		void setValues(const SettingsStore& ss);
		void updateTheme();
		void setBackgroundColor(COLORREF clr, bool updateStyles = false);
		void setCallback(PropPageCallback* p) { callback = p; }

	private:
		class TextStyleSettings : public CHARFORMAT2
		{
			public:
				TextStyleSettings() : parent(nullptr) {}

				void init(ChatStylesTab *parent, ResourceManager::Strings name,
				          SettingsManager::IntSetting bgSetting, SettingsManager::IntSetting fgSetting,
				          SettingsManager::IntSetting boldSetting, SettingsManager::IntSetting italicSetting);
				void loadSettings();
				void loadSettings(const SettingsStore& ss);
				void loadDefaultSettings();
				void saveSettings() const;
				void saveSettings(SettingsStore& ss) const;
				bool editBackgroundColor();
				bool editForegroundColor();
				bool editTextStyle();

				ResourceManager::Strings name;

				ChatStylesTab* parent;
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

		TextStyleSettings textStyles[TS_LAST];
		CListBox lsbList;
		ChatCtrl preview;
		CButton ctrlBoldMsgAuthors;
		tstring currentFont;
		LOGFONT mainFont;
		HFONT hMainFont;
		HBRUSH hBrush;
		COLORREF bg;
		bool boldMsgAuthor;
		PropPageCallback* callback;

		void updateFont();
		void refreshPreview();
		void applyFont();
		void showFont();
		void cleanup();
};

#endif // CHAT_STYLES_TAB_H_

#ifndef USER_LIST_COLORS_H_
#define USER_LIST_COLORS_H_

#include "PropPage.h"

class UserListColors : public CPropertyPage<IDD_USERLIST_COLORS_PAGE>, public PropPage
{
	public:
		explicit UserListColors() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TEXT_STYLES) + _T('\\') + TSTRING(SETTINGS_USER_LIST))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP(UserListColors)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_CHANGE_COLOR, BN_CLICKED, onChangeColor)
		COMMAND_HANDLER(IDC_IMAGEBROWSE, BN_CLICKED, onImageBrowse)
		CHAIN_MSG_MAP(PropPage)
		REFLECT_NOTIFICATIONS()
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onImageBrowse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		int getPageIcon() const { return PROP_PAGE_ICON_USERS; }
		void write();
		void cancel()
		{
			cancel_check();
		}

	private:
		enum
		{
			normalColor,
			favoriteColor,
			favEnemyColor,
			reservedSlotColor,
			ignoredColor,
			fastColor,
			serverColor,
			opColor,
			passiveColor,
			checkedColor,
			badClientColor,
			badFileListColor,
			numberOfColors
		};
		
		uint32_t colors[numberOfColors];

		CListBox ctrlList;
		ExCImage imgUsers;
		CFlyHyperLink linkUsers;
		CRichEditCtrl ctrlPreview;
		CComboBox ctrlHubPosition;
		
		void browseForPic(int dlgItem);		
		void refreshPreview();	
};

#endif // USER_LIST_COLORS_H_

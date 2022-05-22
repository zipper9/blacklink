#ifndef USER_LIST_COLORS_H_
#define USER_LIST_COLORS_H_

#include "PropPage.h"
#include "ResourceLoader.h"

class UserListColors : public CPropertyPage<IDD_USERLIST_COLORS_PAGE>, public PropPage
{
	public:
		explicit UserListColors() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TEXT_STYLES) + _T('\\') + TSTRING(SETTINGS_USER_LIST))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
			customImageLoaded = false;
		}
		
		BEGIN_MSG_MAP(UserListColors)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_CHANGE_COLOR, BN_CLICKED, onChangeColor)
		COMMAND_HANDLER(IDC_IMAGEBROWSE, BN_CLICKED, onImageBrowse)
		COMMAND_HANDLER(IDC_USERLIST, BN_CLICKED, onCustomImage)
		REFLECT_NOTIFICATIONS()
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onImageBrowse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onCustomImage(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

		PROPSHEETPAGE *getPSP() { return (PROPSHEETPAGE *) * this; }
		int getPageIcon() const { return PROP_PAGE_ICON_USERS; }
		void write();

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
		CRichEditCtrl ctrlPreview;
		CComboBox ctrlHubPosition;
		CButton ctrlCustomImage;
		CEdit ctrlImagePath;
		CStatic ctrlUserList;
		CToolTipCtrl tooltip;
		tstring imagePath;
		tstring origImagePath;
		bool customImageLoaded;
		
		void refreshPreview();
		bool loadImage(ExCImage& newImg, const tstring& path, bool showError);
		void loadImage(bool useCustom, bool init);
};

#endif // USER_LIST_COLORS_H_

#ifndef USER_LIST_COLORS_TAB_H_
#define USER_LIST_COLORS_TAB_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include "resource.h"
#include "ResourceLoader.h"
#include "SettingsStore.h"
#include "PropPageCallback.h"

class UserListColorsTab : public CDialogImpl<UserListColorsTab>
{
	public:
		enum { IDD = IDD_USERLIST_TAB };

		UserListColorsTab() : customImageLoaded(false), bg(0), callback(nullptr) {}
		
		BEGIN_MSG_MAP(UserListColorsTab)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_CHANGE_COLOR, BN_CLICKED, onChangeColor)
		COMMAND_HANDLER(IDC_SET_DEFAULT, BN_CLICKED, onSetDefault)
		COMMAND_HANDLER(IDC_IMAGEBROWSE, BN_CLICKED, onImageBrowse)
		COMMAND_HANDLER(IDC_USERLIST, BN_CLICKED, onCustomImage)
		REFLECT_NOTIFICATIONS()
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onSetDefault(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onImageBrowse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onCustomImage(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

		void loadSettings();
		void saveSettings() const;
		void getValues(SettingsStore& ss) const;
		void setValues(const SettingsStore& ss);
		void updateTheme();
		void setBackgroundColor(COLORREF clr);
		void setCallback(PropPageCallback* p) { callback = p; }

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
			checkedFailColor,
			numberOfColors
		};

		uint32_t colors[numberOfColors];
		COLORREF bg;

		CListBox ctrlList;
		CImageEx imgUsers;
		CRichEditCtrl ctrlPreview;
		CComboBox ctrlHubPosition;
		CButton ctrlCustomImage;
		CEdit ctrlImagePath;
		CStatic ctrlUserList;
		CToolTipCtrl tooltip;
		tstring imagePath;
		tstring origImagePath;
		bool customImageLoaded;
		PropPageCallback* callback;
		
		void refreshPreview();
		bool loadImage(CImageEx& newImg, const tstring& path, bool showError);
		void loadImage(bool useCustom, bool init);
};

#endif // USER_LIST_COLORS_TAB_H_

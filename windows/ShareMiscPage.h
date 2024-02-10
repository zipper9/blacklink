/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#ifndef SHARE_MISC_PAGE_H
#define SHARE_MISC_PAGE_H

#include <atlcrack.h>
#include "PropPage.h"
#include "StatusLabelCtrl.h"
#include "../client/CID.h"
#include "../client/Text.h"

class ShareGroupsPage : public CDialogImpl<ShareGroupsPage>
{
	public:
		enum { IDD = IDD_SHARE_MISC_PAGE1 };

		BEGIN_MSG_MAP(ShareGroupsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ADD, onAdd)
		COMMAND_ID_HANDLER(IDC_REMOVE, onAction)
		COMMAND_ID_HANDLER(IDC_RENAME, onAction)
		COMMAND_ID_HANDLER(IDC_SAVE, onSaveChanges)
		COMMAND_HANDLER(IDC_SHARE_GROUPS, CBN_SELCHANGE, onSelectGroup)
		NOTIFY_HANDLER(IDC_LIST1, LVN_GETDISPINFO, onGetDispInfo)
		NOTIFY_HANDLER(IDC_LIST1, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_REMOVE, BCN_DROPDOWN, onSplitAction)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onAdd(WORD, WORD, HWND, BOOL&);
		LRESULT onAction(WORD, WORD, HWND, BOOL&);
		LRESULT onSaveChanges(WORD, WORD, HWND, BOOL&);
		LRESULT onGetDispInfo(int, LPNMHDR pnmh, BOOL&);
		LRESULT onItemChanged(int, LPNMHDR pnmh, BOOL&);
		LRESULT onSplitAction(int, LPNMHDR pnmh, BOOL&);
		LRESULT onSelectGroup(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		void checkShareListVersion(int64_t version);

	private:
		CComboBox ctrlGroup;
		CButton ctrlAction;
		CListViewCtrl ctrlDirs;
		bool changed = false;

		struct ShareGroupInfo
		{
			CID id;
			tstring name;
			int def;
		};

		struct DirInfo
		{
			tstring realPath;
			tstring virtualPath;
		};

		vector<ShareGroupInfo> groups;
		vector<DirInfo> dirs;
		int64_t shareListVersion = 0;
		int action = IDC_REMOVE;

		void sortShareGroups();
		void insertShareGroups(const CID& selId);
		void insertDirectories();
		void showShareGroupDirectories(int index);
		void updateShareGroup(int index, const tstring& newName);
};

class ShareOptionsPage : public CDialogImpl<ShareOptionsPage>
{
	public:
		enum { IDD = IDD_SHARE_MISC_PAGE2 };

		BEGIN_MSG_MAP(ShareOptionsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_TTH_IN_STREAM, onFixControls)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD, WORD, HWND, BOOL&) { fixControls(); return 0; }
		void fixControls();
};

class ShareMediaInfoPage : public CDialogImpl<ShareMediaInfoPage>
{
	public:
		enum { IDD = IDD_SHARE_MISC_PAGE3 };

		BEGIN_MSG_MAP(ShareMediaInfoPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ENABLE, onEnable)
		END_MSG_MAP()

		StatusLabelCtrl ctrlStatus;
		CButton ctrlEnable;
		CButton ctrlParseAudio;
		CButton ctrlParseVideo;
		CButton ctrlForceUpdate;

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onEnable(WORD, WORD, HWND, BOOL&);
		void readSettings();
		void writeSettings();
		void fixControls();
		void updateStatus();

	private:
		void disableControls();
		void showStatus();
};

class ShareMiscPage : public CPropertyPage<IDD_SHARE_MISC_PAGE>, public PropPage
{
	public:
		explicit ShareMiscPage() : PropPage(TSTRING(SETTINGS_UPLOADS) + _T('\\') + TSTRING(SETTINGS_ADVANCED))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}

		BEGIN_MSG_MAP(ShareMiscPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, onChangeTab)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeTab(int idCtrl, LPNMHDR pnmh, BOOL& bHandled) { changeTab(); return 1; }

		PROPSHEETPAGE *getPSP() { return (PROPSHEETPAGE *) *this; }
		int getPageIcon() const { return PROP_PAGE_ICON_UPLOAD_ADVANCED; }
		void onShow() override;
		void write();

	private:
		CTabCtrl ctrlTabs;
		std::unique_ptr<ShareGroupsPage> pageShareGroups;
		std::unique_ptr<ShareOptionsPage> pageShareOptions;
		std::unique_ptr<ShareMediaInfoPage> pageShareMediaInfo;

		void changeTab();
};

#endif //SHARE_MISC_PAGE_H

#ifndef USER_LIST_WINDOW_H_
#define USER_LIST_WINDOW_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include "TypedListViewCtrl.h"
#include "CustomDrawHelpers.h"
#include "UserInfo.h"
#include "UserInfoBaseHandler.h"
#include "SearchBoxCtrl.h"
#include "../client/TaskQueue.h"
#include "../client/FavoriteManager.h"
#include "../client/OnlineUserParams.h"

class UserListWindow : public CWindowImpl<UserListWindow>
{
	public:
		struct HubFrameCallbacks
		{
			virtual void showErrorMessage(const tstring& text) = 0;
			virtual void setCurrentNick(const tstring& nick) = 0;
			virtual void appendNickToChat(const tstring& nick) = 0;
			virtual void addTask(int type, Task* task) = 0;
			virtual void updateUserCount() = 0;
		};

		DECLARE_WND_CLASS_EX(_T("UserListWindow"), CS_HREDRAW | CS_VREDRAW, COLOR_3DFACE);

		UserListWindow(HubFrameCallbacks* hubFrame);
		~UserListWindow();

		UserListWindow(const UserListWindow&) = delete;
		UserListWindow& operator= (const UserListWindow&) = delete;

		typedef TypedListViewCtrl<UserInfo> CtrlUsers;

		CtrlUsers& getUserList() { return ctrlUsers; }
		void setHubHint(const string& hint);
		void setShowUsers(bool flag);
		void setShowHidden(bool flag);
		void initialize(const FavoriteManager::WindowInfo& wi);
		void clearUserList();
		void insertDHTUsers();
		UserInfo* findUser(const OnlineUserPtr& user) const;
		bool updateUser(const OnlineUserPtr& ou, uint32_t columnMask, bool isConnected); // returns true if this is a new user
		void removeUser(const OnlineUserPtr& ou);
		void ensureVisible(const UserInfo* ui);
		void setSortFlag() { shouldSort = true; }
		bool checkSortFlag();
		void updateLayout();
		bool showHeaderMenu(POINT pt);
		UserInfo* getSelectedUserInfo(bool* isMultiple) const;
		void getSelectedUsers(vector<OnlineUserPtr>& v) const;
		bool selectNick(const tstring& nick);
		bool selectCID(const CID& cid);
		void getDupUsers(const OnlineUserParams& param, const tstring& hubTitle, const string& hubUrl, UINT& idc, vector<UserInfoGuiTraits::DetailsItem>& items) const;
		bool loadIPInfo(const OnlineUserPtr& ou);

		void onIgnoreListChanged();
		void onIgnoreListCleared();

	private:
		BEGIN_MSG_MAP(UserListWindow)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_NEXTDLGCTL, onNextDlgCtl)
		MESSAGE_HANDLER(WMU_RETURN, onFilterReturn)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETDISPINFO, ctrlUsers.onGetDispInfo)
		NOTIFY_HANDLER(IDC_USERS, LVN_COLUMNCLICK, ctrlUsers.onColumnClick)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETINFOTIP, ctrlUsers.onInfoTip)
		NOTIFY_HANDLER(IDC_USERS, NM_DBLCLK, onDoubleClickUsers)
		NOTIFY_HANDLER(IDC_USERS, NM_RETURN, onEnterUsers)
		NOTIFY_HANDLER(IDC_USERS, NM_CUSTOMDRAW, onCustomDraw)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onSelChange)
		COMMAND_CODE_HANDLER(EN_CHANGE, onFilterChanged)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onNextDlgCtl(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onFilterReturn(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDoubleClickUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onEnterUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onFilterChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	private:
		enum FilterModes
		{
			NONE,
			EQUAL,
			GREATER_EQUAL,
			LESS_EQUAL,
			GREATER,
			LESS,
			NOT_EQUAL
		};

		UserInfo::OnlineUserMap userMap;

		CtrlUsers ctrlUsers;
		CustomDrawHelpers::CustomDrawState customDrawState;
		static const int columnId[COLUMN_LAST];

		SearchBoxCtrl ctrlSearchBox;
		CComboBox ctrlFilterSel;

		int xdu, ydu;

		int filterSelPos;
		tstring filter;
		tstring filterLower;

		bool showUsers;
		bool showHidden;
		bool shouldSort;
		bool isOp;

		HubFrameCallbacks* hubFrame;
		string hubHint;

		int getFilterSelPos() const
		{
			return ctrlFilterSel.m_hWnd ? ctrlFilterSel.GetCurSel() : filterSelPos;
		}
		size_t insertUsers();

		bool insertUser(UserInfo* ui);
		void insertUserInternal(UserInfo* ui, int pos);
		void updateUserList();
		void removeListViewItems();
		void selectItem(int pos);
		bool shouldShowUser(const UserInfo* ui) const;

		bool parseFilter(FilterModes& mode, int64_t& size);
		bool matchFilter(UserInfo& ui, int sel, bool doSizeCompare = false, FilterModes mode = NONE, int64_t size = 0);
};

#endif // USER_LIST_WINDOW_H_

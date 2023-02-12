#ifndef RECENTS_FRAME_H_
#define RECENTS_FRAME_H_

#include "FlatTabCtrl.h"
#include "StaticFrame.h"
#include "TypedListViewCtrl.h"
#include "../client/HubEntry.h"
#include "../client/FavoriteManagerListener.h"

class RecentHubsFrame : public MDITabChildWindowImpl<RecentHubsFrame>,
	public StaticFrame<RecentHubsFrame, ResourceManager::RECENT_HUBS, IDC_RECENTS>,
	private FavoriteManagerListener,
	private SettingsManagerListener,
	public CMessageFilter
{
		struct ItemInfo
		{
			RecentHubEntry* const entry;

			ItemInfo(RecentHubEntry* entry) : entry(entry) {}
			tstring getText(uint8_t col) const;
			static int compareItems(const ItemInfo* a, const ItemInfo* b, uint8_t col);
			static int getImageIndex() { return 0; }
			static int getStateImageIndex() { return 0; }
		};

	public:
		typedef MDITabChildWindowImpl<RecentHubsFrame> baseClass;

		RecentHubsFrame();

		RecentHubsFrame(const RecentHubsFrame &) = delete;
		RecentHubsFrame& operator=(const RecentHubsFrame &) = delete;

		static CFrameWndClassInfo& GetWndClassInfo();

		BEGIN_MSG_MAP(RecentHubsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(FTM_GETOPTIONS, onTabGetOptions)
		COMMAND_ID_HANDLER(IDC_CONNECT, onClickedConnect)
		COMMAND_ID_HANDLER(IDC_ADD, onAddFav)
		COMMAND_ID_HANDLER(IDC_REM_AS_FAVORITE, onRemoveFav)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVE_ALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		NOTIFY_HANDLER(IDC_RECENTS, LVN_GETDISPINFO, ctrlHubs.onGetDispInfo)
		NOTIFY_HANDLER(IDC_RECENTS, LVN_COLUMNCLICK, ctrlHubs.onColumnClick)
		NOTIFY_HANDLER(IDC_RECENTS, LVN_GETINFOTIP, ctrlHubs.onInfoTip)
		NOTIFY_HANDLER(IDC_RECENTS, NM_DBLCLK, onDoubleClickHublist)
		/*
		NOTIFY_HANDLER(IDC_RECENTS, NM_RETURN, onEnter)
		*/
		NOTIFY_HANDLER(IDC_RECENTS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_RECENTS, LVN_ITEMCHANGED, onItemChanged)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDoubleClickHublist(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onAddFav(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemoveFav(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onClickedConnect(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemoveAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onTabGetOptions(UINT, WPARAM, LPARAM lParam, BOOL&);

		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}

		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		void UpdateLayout(BOOL bResizeBars = TRUE);

		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);

		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlHubs.SetFocus();
			return 0;
		}

		virtual BOOL PreTranslateMessage(MSG* pMsg) override;

	private:
		enum
		{
			COLUMN_NAME,
			COLUMN_DESCRIPTION,
			COLUMN_USERS,
			COLUMN_SHARED,
			COLUMN_SERVER,
			COLUMN_LAST_SEEN,
			COLUMN_OPEN_TAB,
			COLUMN_LAST
		};

		static const int columnId[COLUMN_LAST];

		CButton ctrlConnect;
		CButton ctrlRemove;
		CButton ctrlRemoveAll;
		OMenu hubsMenu;
		TypedListViewCtrl<ItemInfo, IDC_RECENTS> ctrlHubs;

		int xdu, ydu;
		int buttonWidth, buttonHeight, buttonSpace;
		int vertMargin, horizMargin;

		void updateList(const RecentHubEntry::List& fl);
		void removeSelectedItems();
		void addEntry(RecentHubEntry* entry);
		void openHubWindow(RecentHubEntry* entry);

		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

		void on(RecentAdded, const RecentHubEntry* entry) noexcept override
		{
			addEntry(const_cast<RecentHubEntry*>(entry));
		}
		void on(RecentRemoved, const RecentHubEntry* entry) noexcept override;
		void on(RecentUpdated, const RecentHubEntry* entry) noexcept override;

		void on(SettingsManagerListener::Repaint) override;
};

#endif

#ifndef DIRECTORY_LISTING_NAV_WND_H
#define DIRECTORY_LISTING_NAV_WND_H

#include "NavigationBar.h"
#include "../client/DirectoryListing.h"

class DirectoryListingFrame;

class DirectoryListingNavWnd : public CWindowImpl<DirectoryListingNavWnd>
{
	public:
		enum
		{
			PATH_ITEM_ROOT,
			PATH_ITEM_FOLDER
		};

		DirectoryListingNavWnd();
		int updateNavBarHeight();
		int getWindowHeight() const { return isVisible ? navBarHeight : 0; }
		void updateNavBar(const DirectoryListing::Directory* dir, const string& path, const DirectoryListingFrame* frame);
		void enableNavigationButton(int idc, bool enable);
		void enableControlButton(int idc, bool enable);
		void initToolbars(const DirectoryListingFrame* frame);

		CToolBarCtrl ctrlNavigation;
		CToolBarCtrl ctrlButtons;
		NavigationBar navBar;

	private:
		int navBarHeight;
		int navBarMinWidth;
		int space;
		bool isAppThemed;
		bool isVisible;

		BEGIN_MSG_MAP(DirectoryListingNavWnd)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBackground)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_COMMAND, onCommand)
		MESSAGE_HANDLER(WM_THEMECHANGED, onThemeChanged)
		END_MSG_MAP()

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCommand(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);

		static void hideControl(HWND hWnd);
};

#endif // DIRECTORY_LISTING_NAV_WND_H

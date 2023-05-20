#ifndef BASE_HANDLERS_H_
#define BASE_HANDLERS_H_

#include "PreviewMenu.h"
#include "SearchUrl.h"
#include "resource.h"
#include "../client/Text.h"

class PreviewBaseHandler : public PreviewMenu
{
		/*
		1) Implement onPreviewCommand in your class, which will call runPreview for the selected item.
		2) clearPreviewMenu()
		3) appendPreviewItems(yourMenu)
		4) setupPreviewMenu(yourMenu)
		5) activatePreviewItems(yourMenu)
		6) Before destroying the menu in your class call WinUtil::unlinkStaticMenus(yourMenu)
		*/
	protected:
		BEGIN_MSG_MAP(PreviewBaseHandler)
		COMMAND_RANGE_HANDLER(IDC_PREVIEW_APP, IDC_PREVIEW_APP + MAX_PREVIEW_APPS - 1, onPreviewCommand)
		END_MSG_MAP()

		virtual LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;

		static void appendPreviewItems(OMenu& menu);
		static void activatePreviewItems(OMenu& menu);
};

class InternetSearchBaseHandler
{
	protected:
		static const int MAX_WEB_SEARCH_URLS = 100;

		BEGIN_MSG_MAP(InternetSearchBaseHandler)
		COMMAND_RANGE_HANDLER(IDC_WEB_SEARCH, IDC_WEB_SEARCH + MAX_WEB_SEARCH_URLS - 1, onPerformWebSearch)
		END_MSG_MAP()

		virtual LRESULT onPerformWebSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;

		static int appendWebSearchItems(OMenu& menu, SearchUrl::Type type, bool subMenu, ResourceManager::Strings subMenuTitle);
		static void performWebSearch(WORD wID, const string& query);
		static void performWebSearch(const string& urlTemplate, const string& query);
		static int getWebSearchType(WORD wID);
};

#endif // BASE_HANDLERS_H_

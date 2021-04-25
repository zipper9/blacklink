#ifndef BASE_HANDLERS_H_
#define BASE_HANDLERS_H_

#include "PreviewMenu.h"
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
		static const int MAX_PREVIEW_APPS = 100;

		BEGIN_MSG_MAP(PreviewBaseHandler)
		COMMAND_RANGE_HANDLER(IDC_PREVIEW_APP, IDC_PREVIEW_APP + MAX_PREVIEW_APPS - 1, onPreviewCommand)
		END_MSG_MAP()
		
		virtual LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;
		
		static void appendPreviewItems(OMenu& menu);
		static void activatePreviewItems(OMenu& menu);
};

class InternetSearchBaseHandler
{
		/*
		1) Implement onSearchFileInInternet in your class, which will call searchFileInInternet for the selected item.
		2) appendInternetSearchItems(yourMenu)
		*/
	protected:
		BEGIN_MSG_MAP(InternetSearchBaseHandler)
		COMMAND_ID_HANDLER(IDC_SEARCH_FILE_IN_GOOGLE, onSearchFileOnInternet)
		COMMAND_ID_HANDLER(IDC_SEARCH_FILE_IN_YANDEX, onSearchFileOnInternet)
		END_MSG_MAP()

		virtual LRESULT onSearchFileOnInternet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) = 0;

		void appendInternetSearchItems(OMenu& menu);
		static void searchFileOnInternet(const WORD wID, const tstring& file);

		static void searchFileOnInternet(const WORD wID, const string& file)
		{
			searchFileOnInternet(wID, Text::toT(file));
		}
};

#endif // BASE_HANDLERS_H_

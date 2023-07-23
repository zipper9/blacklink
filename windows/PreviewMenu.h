#ifndef PREVIEW_MENU_H_
#define PREVIEW_MENU_H_

#include "OMenu.h"
#include "../client/forward.h"
#include "../client/HashValue.h"
#include "../client/PreviewApplication.h"

class PreviewMenu
{
	public:
		static const int MAX_PREVIEW_APPS = 100;
		static void init() { previewMenu.CreatePopupMenu(); }
		static bool isPreviewMenu(HMENU handle) { return previewMenu.m_hMenu == handle; }
		static void detectApps(PreviewApplication::List& apps);

	protected:
		static void runPreview(WORD wID, const QueueItemPtr& qi);
		static void runPreview(WORD wID, const TTHValue& tth);
		static void runPreview(WORD wID, const string& target);

		static void clearPreviewMenu();

		static void setupPreviewMenu(const string& target);
		static void runPreviewCommand(WORD wID, const string& target);

		static int previewAppsSize;
		static OMenu previewMenu;

		dcdrun(static bool _debugIsClean;)
};

#endif // PREVIEW_MENU_H_

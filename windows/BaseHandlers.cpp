#include "stdafx.h"
#include "BaseHandlers.h"
#include "ImageLists.h"
#include "WinUtil.h"
#include "../client/ParamExpander.h"

void PreviewBaseHandler::appendPreviewItems(OMenu& menu)
{
	dcassert(_debugIsClean);
	dcdrun(_debugIsClean = false;)

	menu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU) previewMenu, CTSTRING(PREVIEW_MENU), g_iconBitmaps.getBitmap(IconBitmaps::PREVIEW, 0));
}

void PreviewBaseHandler::activatePreviewItems(OMenu& menu)
{
	dcassert(!_debugIsActivated);
	dcdrun(_debugIsActivated = true;)

	int count = menu.GetMenuItemCount();
	MENUITEMINFO mii = { sizeof(mii) };
	// Passing HMENU to EnableMenuItem doesn't work with owner-draw OMenus for some reason
	mii.fMask = MIIM_SUBMENU;
	for (int i = 0; i < count; ++i)
		if (menu.GetMenuItemInfo(i, TRUE, &mii) && mii.hSubMenu == (HMENU) previewMenu)
		{
			menu.EnableMenuItem(i, MF_BYPOSITION | (previewMenu.GetMenuItemCount() > 0 ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
			break;
		}
}

class WebSearchParamExpander : public Util::ParamExpander
{
	public:
		WebSearchParamExpander(const string& query) : query(query) {}

		virtual const string& expandBracket(const string& param) noexcept
		{
			return param == "s" ? query : Util::emptyString;
		}

		virtual const string& expandCharSequence(const string& str, string::size_type pos, string::size_type& usedChars) noexcept
		{
			if (str[pos] == 's')
			{
				usedChars = 1;
				return query;
			}
			usedChars = 0;
			return Util::emptyString;
		}

	private:
		const string query;
};


int InternetSearchBaseHandler::appendWebSearchItems(OMenu& menu, SearchUrl::Type type, bool subMenu, ResourceManager::Strings subMenuTitle)
{
	const auto& data = FavoriteManager::getInstance()->getSearchUrls();
	int result = 0;
	size_t count = std::min<size_t>(data.size(), MAX_WEB_SEARCH_URLS);
	CMenu sub;
	for (size_t i = 0; i < count; ++i)
		if (data[i].type == type)
		{
			if (subMenu)
			{
				if (!sub.m_hMenu) sub.CreatePopupMenu();
				sub.AppendMenu(MF_STRING, IDC_WEB_SEARCH + i, Text::toT(data[i].description).c_str());
			}
			else
				menu.AppendMenu(MF_STRING, IDC_WEB_SEARCH + i, Text::toT(data[i].description).c_str(), g_iconBitmaps.getBitmap(IconBitmaps::WEB_SEARCH, 0));
			++result;
		}
	if (sub.m_hMenu)
		menu.AppendMenu(MF_POPUP, sub.Detach(), CTSTRING_I(subMenuTitle), g_iconBitmaps.getBitmap(IconBitmaps::WEB_SEARCH, 0));
	return result;
}

void InternetSearchBaseHandler::performWebSearch(WORD wID, const string& query)
{
	if (query.empty() || wID < IDC_WEB_SEARCH) return;
	size_t pos = wID - IDC_WEB_SEARCH;
	const auto& data = FavoriteManager::getInstance()->getSearchUrls();
	if (pos >= data.size()) return;
	WebSearchParamExpander ex(Util::encodeUriQuery(query));
	const string& urlTemplate = data[pos].url;
	string url = Util::formatParams(urlTemplate, &ex, false);
	if (url != urlTemplate) WinUtil::openFile(Text::toT(url));
}

int InternetSearchBaseHandler::getWebSearchType(WORD wID)
{
	if (wID < IDC_WEB_SEARCH) return -1;
	size_t pos = wID - IDC_WEB_SEARCH;
	const auto& data = FavoriteManager::getInstance()->getSearchUrls();
	if (pos >= data.size()) return -1;
	return (int) data[pos].type;
}

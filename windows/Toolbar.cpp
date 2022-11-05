#include "stdafx.h"
#include "Toolbar.h"
#include "resource.h"
#include "../client/StrUtil.h"
#include "../client/SimpleStringTokenizer.h"

// ToolbarButton::image values MUST be in order without gaps.
const ToolbarButton g_ToolbarButtons[] =
{
	{ IDC_PUBLIC_HUBS,            true,  ResourceManager::MENU_PUBLIC_HUBS            },
	{ IDC_RECONNECT,              false, ResourceManager::MENU_RECONNECT              },
	{ IDC_FOLLOW,                 false, ResourceManager::MENU_FOLLOW_REDIRECT        },
	{ IDC_FAVORITES,              true,  ResourceManager::MENU_FAVORITE_HUBS          },
	{ IDC_FAVUSERS,               true,  ResourceManager::MENU_FAVORITE_USERS         },
	{ IDC_RECENTS,                true,  ResourceManager::MENU_FILE_RECENT_HUBS       },
	{ IDC_QUEUE,                  true,  ResourceManager::MENU_DOWNLOAD_QUEUE         },
	{ IDC_FINISHED,               true,  ResourceManager::MENU_FINISHED_DOWNLOADS     },
	{ IDC_UPLOAD_QUEUE,           true,  ResourceManager::MENU_WAITING_USERS          },
	{ IDC_FINISHED_UL,            true,  ResourceManager::MENU_FINISHED_UPLOADS       },
	{ ID_FILE_SEARCH,             false, ResourceManager::MENU_SEARCH                 },
	{ IDC_FILE_ADL_SEARCH,        true,  ResourceManager::MENU_ADL_SEARCH             },
	{ IDC_SEARCH_SPY,             true,  ResourceManager::MENU_SEARCH_SPY             },
	{ IDC_NET_STATS,              true,  ResourceManager::NETWORK_STATISTICS          },
	{ IDC_OPEN_MY_LIST,           false, ResourceManager::MENU_OPEN_OWN_LIST          },
	{ ID_FILE_SETTINGS,           false, ResourceManager::MENU_SETTINGS               },
	{ IDC_NOTEPAD,                true,  ResourceManager::MENU_NOTEPAD                },
	{ IDC_AWAY,                   true,  ResourceManager::AWAY                        },
	{ IDC_SHUTDOWN,               true,  ResourceManager::SHUTDOWN                    },
	{ IDC_LIMITER,                true,  ResourceManager::SETCZDC_ENABLE_LIMITING     },
	{ -1,                         false, ResourceManager::Strings()                   },
	{ IDC_DISABLE_SOUNDS,         true,  ResourceManager::DISABLE_SOUNDS              },
	{ IDC_OPEN_DOWNLOADS,         false, ResourceManager::MENU_OPEN_DOWNLOADS_DIR     },
	{ IDC_REFRESH_FILE_LIST,      false, ResourceManager::MENU_REFRESH_FILE_LIST      },
	{ ID_VIEW_MEDIA_TOOLBAR,      true,  ResourceManager::TOGGLE_TOOLBAR              },
	{ IDC_QUICK_CONNECT,          false, ResourceManager::MENU_QUICK_CONNECT          },
	{ IDC_OPEN_FILE_LIST,         false, ResourceManager::MENU_OPEN_FILE_LIST         },
	{ IDC_RECONNECT_DISCONNECTED, false, ResourceManager::MENU_RECONNECT_DISCONNECTED },
	{ -1,                         true,  ResourceManager::Strings()                   },
	{ IDC_DISABLE_POPUPS,         true,  ResourceManager::DISABLE_POPUPS              },
	{ 0,                          false, ResourceManager::MENU_NOTEPAD                }
};

const ToolbarButton g_WinampToolbarButtons[] =
{
	{ IDC_WINAMP_SPAM,     false, ResourceManager::WINAMP_SPAM     },
	{ IDC_WINAMP_BACK,     false, ResourceManager::WINAMP_BACK     },
	{ IDC_WINAMP_PLAY,     false, ResourceManager::WINAMP_PLAY     },
	{ IDC_WINAMP_PAUSE,    false, ResourceManager::WINAMP_PAUSE    },
	{ IDC_WINAMP_NEXT,     false, ResourceManager::WINAMP_NEXT     },
	{ IDC_WINAMP_STOP,     false, ResourceManager::WINAMP_STOP     },
	{ IDC_WINAMP_VOL_DOWN, false, ResourceManager::WINAMP_VOL_DOWN },
	{ IDC_WINAMP_VOL_HALF, false, ResourceManager::WINAMP_VOL_HALF },
	{ IDC_WINAMP_VOL_UP,   false, ResourceManager::WINAMP_VOL_UP   },
	{ 0,                   false, ResourceManager::WINAMP_PLAY     }
};

const MenuImage g_MenuImages[] =
{
	{IDC_CDMDEBUG_WINDOW,    30},
	{ID_WINDOW_CASCADE,      31},
	{ID_WINDOW_TILE_HORZ,    32},
	{ID_WINDOW_TILE_VERT,    33},
	{ID_WINDOW_MINIMIZE_ALL, 34},
	{ID_WINDOW_RESTORE_ALL,  35},
	{ID_GET_TTH,             36},
	{IDC_MATCH_ALL,          37},
	{ID_APP_EXIT,            38},
	{IDC_HASH_PROGRESS,      39},
	{ID_WINDOW_ARRANGE,      40},
	{-1,                     41},
	{-1,                     42},
	{IDC_SHUTDOWN,           43},
	{IDC_DHT,                44},
	{ID_APP_ABOUT,           45},
	{-1,                     46},
	{IDC_ADD_MAGNET,         47},
	{0,  0}
};

const int g_ToolbarButtonsCount = _countof(g_ToolbarButtons);
const int g_WinampToolbarButtonsCount = _countof(g_WinampToolbarButtons);

void fillToolbarButtons(CToolBarCtrl& toolbar, const string& setting, const ToolbarButton* buttons, int buttonCount, const uint8_t* checkState)
{
	toolbar.SetButtonStructSize();
	SimpleStringTokenizer<char> t(setting, ',');
	string tok;
	while (t.getNextToken(tok))
	{
		const int i = Util::toInt(tok);
		if (i < buttonCount)
		{
				TBBUTTON tbb = {0};
				if (i < 0)
				{
					tbb.fsStyle = TBSTYLE_SEP;
				}
				else
				{
					if (buttons[i].id < 0) continue;
					tbb.iBitmap = i;
					tbb.idCommand = buttons[i].id;
					tbb.fsState = TBSTATE_ENABLED;
					tbb.fsStyle = TBSTYLE_BUTTON;
					if (buttons[i].check)
					{
						tbb.fsStyle = TBSTYLE_CHECK;
						if (checkState) tbb.fsState |= checkState[i];
					}
					tbb.iString = (INT_PTR)(CTSTRING_I(buttons[i].tooltip));
					dcassert(tbb.iString != -1);
					if (tbb.idCommand  == IDC_WINAMP_SPAM)
						tbb.fsStyle |= TBSTYLE_DROPDOWN;
				}
				toolbar.AddButtons(1, &tbb);
		}
	}
}

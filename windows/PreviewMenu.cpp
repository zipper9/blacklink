#include "stdafx.h"
#include "PreviewMenu.h"
#include "RegKey.h"
#include "resource.h"
#include "../client/FavoriteManager.h"
#include "../client/ParamExpander.h"
#include "../client/LogManager.h"
#include "../client/QueueItem.h"
#include "../client/ShareManager.h"
#include "../client/SimpleStringTokenizer.h"

OMenu PreviewMenu::previewMenu;
int PreviewMenu::previewAppsSize = 0;
dcdrun(bool PreviewMenu::_debugIsClean = true;)

static const char* extAudio =
	"mp3;ogg;wav;wma;flac";

static const char* extMedia =
	"avi;mkv;mp4;mov;wmv;webm;mpg;mpeg;ts;m2ts;mts;"
	"vob;divx;flv;asf;ogv;3gp;3g2;"
	"mp3;ogg;wav;wma;flac";

struct KnownApp
{
	const TCHAR* name;
	const TCHAR* exeNames;
	const char* extensions;
};

static const KnownApp knownApps[] =
{
	{ _T("VLC Media Player"), _T("vlc.exe"), extMedia },
	{ _T("MPC Home Cinema"), _T("mpc-hc64.exe;mpc-hc.exe"), extMedia },
	{ _T("Windows Media Player"),  _T("wmplayer.exe"), extMedia },
	{ _T("Winamp"), _T("winamp.exe"), extAudio }
};

void PreviewMenu::detectApps(PreviewApplication::List& apps)
{
	WinUtil::RegKey parentKey;
	if (!parentKey.open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths"), KEY_READ)) return;
	tstring exeNames, exeName, appPath;
	for (int i = 0; i < _countof(knownApps); ++i)
	{
		exeNames = knownApps[i].exeNames;
		SimpleStringTokenizer<TCHAR> st(exeNames, _T(';'));
		while (st.getNextToken(exeName))
		{
			WinUtil::RegKey key;
			if (key.open(parentKey.getKey(), exeName.c_str(), KEY_READ) &&
				key.readString(nullptr, appPath) && !appPath.empty() &&
				File::isExist(appPath))
			{
				apps.push_back(new PreviewApplication(
					Text::fromT(knownApps[i].name), Text::fromT(appPath),
					"%[file]", knownApps[i].extensions));
				break;
			}
		}
	}
}

void PreviewMenu::setupPreviewMenu(const string& target)
{
	dcassert(previewAppsSize == 0);
	previewAppsSize = 0;
	
	const auto targetLower = Text::toLower(target);
	
	const auto& lst = FavoriteManager::getInstance()->getPreviewApps();
	size_t size = std::min<size_t>(lst.size(), MAX_PREVIEW_APPS);
	for (size_t i = 0; i < size; ++i)
	{
		const auto tok = Util::splitSettingAndLower(lst[i]->extension);
		if (tok.empty())
		{
			previewMenu.AppendMenu(MF_STRING, IDC_PREVIEW_APP + i, Text::toT(lst[i]->name).c_str());
			previewAppsSize++;
		}
		else
			for (auto si = tok.cbegin(); si != tok.cend(); ++si)
			{
				if (Util::checkFileExt(targetLower, *si))
				{
					previewMenu.AppendMenu(MF_STRING, IDC_PREVIEW_APP + i, Text::toT(lst[i]->name).c_str());
					previewAppsSize++;
					break;
				}
			}
	}
}

// FIXME
template <class string_type>
static void AppendQuotesToPath(string_type& path)
{
	if (path.length() < 1)
		return;
		
	if (path[0] != '"')
		path = '\"' + path;
		
	if (path.back() != '"')
		path += '\"';
}

void PreviewMenu::runPreviewCommand(WORD wID, const string& file)
{
	if (wID < IDC_PREVIEW_APP) return;
	wID -= IDC_PREVIEW_APP;
	const auto& lst = FavoriteManager::getInstance()->getPreviewApps();
	if (wID >= lst.size()) return;

	const auto& application = lst[wID]->application;
	const auto& arguments = lst[wID]->arguments;
	StringMap fileParams;
		
	string dir = Util::getFilePath(file);
	AppendQuotesToPath(dir);
	fileParams["dir"] = dir;

	string quotedFile = file;
	AppendQuotesToPath(quotedFile);
	fileParams["file"] = quotedFile;
	
	string expandedArguments = Util::formatParams(arguments, fileParams, false);
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message("Running command: " + application + " " + expandedArguments, false);
	::ShellExecute(NULL, NULL, Text::toT(application).c_str(), Text::toT(expandedArguments).c_str(), Text::toT(dir).c_str(), SW_SHOWNORMAL);
}

void PreviewMenu::runPreview(WORD wID, const QueueItemPtr& qi)
{
	qi->lockAttributes();
	string path = qi->getTempTargetL();
	qi->unlockAttributes();
	if (path.empty())
		path = qi->getTarget();
	runPreviewCommand(wID, path);
}

void PreviewMenu::runPreview(WORD wID, const TTHValue& tth)
{
	string path;
	if (ShareManager::getInstance()->getFileInfo(tth, path))
		runPreview(wID, path);
}

void PreviewMenu::runPreview(WORD wID, const string& target)
{
	if (!target.empty())
		runPreviewCommand(wID, target);
}

void PreviewMenu::clearPreviewMenu()
{
	previewMenu.ClearMenu();
	previewAppsSize = 0;
	dcdrun(_debugIsClean = true;)
}

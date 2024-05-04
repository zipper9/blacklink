#include "stdafx.h"
#include "PreviewMenu.h"
#include "RegKey.h"
#include "resource.h"
#include "../client/PathUtil.h"
#include "../client/FavoriteManager.h"
#include "../client/ParamExpander.h"
#include "../client/LogManager.h"
#include "../client/QueueItem.h"
#include "../client/ShareManager.h"
#include "../client/SimpleStringTokenizer.h"

OMenu PreviewMenu::previewMenu;
int PreviewMenu::previewAppsSize = 0;

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

class PreviewParamsExpander : public Util::TimeParamExpander
{
	public:
		PreviewParamsExpander(const string& path) :
			TimeParamExpander(time(nullptr)),
			path(path), dir(Util::getFilePath(path))
		{
		}

		virtual const string& expandBracket(const string& str, string::size_type pos, string::size_type endPos) noexcept
		{
			string param = str.substr(pos, endPos - pos);
			if (param == "file") return maybeQuote(path, str, pos, endPos);
			if (param == "dir") return maybeQuote(dir, str, pos, endPos);
			return Util::emptyString;
		}

		const string& getDir() const { return dir; }

		const string& maybeQuote(const string& subst, const string& str, string::size_type pos, string::size_type endPos)
		{
			char c = pos >= 3 ? str[pos-3] : 0;
			bool hasQuotes = c == '"';
			if (!hasQuotes)
			{
				c = endPos + 1 < str.length() ? str[endPos+1] : 0;
				hasQuotes = c == '"';
			}
			bool insertQuotes = !hasQuotes && subst.find_first_of(" \"") != string::npos;
			if (!hasQuotes && !insertQuotes) return subst;
			tmp = subst;
			escapeCommandLineParam(tmp);
			if (insertQuotes)
			{
				tmp.insert(0, 1, '"');
				tmp += '"';
			}
			return tmp;
		}

		static void escapeCommandLineParam(string& s)
		{
			string::size_type startPos = 0;
			while (startPos < s.length())
			{
				string::size_type pos = s.find_first_of("\"\\", startPos);
				if (pos == string::npos) break;
				if (s[pos] == '\\')
				{
					string::size_type oldPos = pos;
					char c = '\\';
					for (++pos; pos < s.length(); ++pos)
					{
						c = s[pos];
						if (c != '\\') break;
					}
					if (c == '"' || c == '\\')
					{
						size_t count = pos - oldPos;
						if (c == '"') count++;
						s.insert(oldPos, count, '\\');
						pos += count;
					}
				}
				else if (s[pos] == '"')
				{
					s.insert(pos, 1, '\\');
					pos++;
				}
				startPos = pos + 1;
			}
		}

	private:
		const string path;
		const string dir;
		string tmp;
};

void PreviewMenu::runPreviewCommand(WORD wID, const string& file)
{
	if (wID < IDC_PREVIEW_APP) return;
	wID -= IDC_PREVIEW_APP;
	const auto& lst = FavoriteManager::getInstance()->getPreviewApps();
	if (wID >= lst.size()) return;

	const auto& application = lst[wID]->application;
	const auto& arguments = lst[wID]->arguments;
	PreviewParamsExpander ex(file);
	string expandedArguments = Util::formatParams(arguments, &ex, false);

	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message("Running command: " + application + " " + expandedArguments, false);
	::ShellExecute(NULL, NULL, Text::toT(application).c_str(), Text::toT(expandedArguments).c_str(), Text::toT(ex.getDir()).c_str(), SW_SHOWNORMAL);
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
}

#include "stdafx.h"
#include "MainFrm.h"
#include "SearchFrm.h"
#include "Players.h"

#ifdef DEBUG_GDI_IMAGE
#include "../GdiOle/GDIImage.h"
#endif

using namespace Commands;

static void openLogFile(int area, Commands::Result& res)
{
	string filename = LogManager::getLogFileName(area, StringMap());
	if (!File::isExist(filename))
	{
		res.text = STRING(COMMAND_NO_LOG_FILE);
		res.what = RESULT_ERROR_MESSAGE;
		return;
	}
	WinUtil::openFile(Text::toT(filename));
	res.what = RESULT_NO_TEXT;
}

static void checkPlayerMessage(string& text, Result& res, ResourceManager::Strings msgNotPlaying, ResourceManager::Strings msgNotRunning)
{
	if (text.empty())
	{
		res.text = STRING_I(msgNotRunning);
		res.what = RESULT_ERROR_MESSAGE;
		return;
	}
	if (text == "no_media")
	{
		res.text = STRING_I(msgNotPlaying);
		res.what = RESULT_ERROR_MESSAGE;
		return;
	}
	res.text = std::move(text);
	res.what = RESULT_TEXT;
}

static const string& getFirstWebSearchUrl(int type)
{
	const auto& urls = FavoriteManager::getInstance()->getSearchUrls();
	for (const auto& url : urls)
		if (url.type == type)
			return url.url;
	return Util::emptyString;
}

bool MainFrame::processCommand(const ParsedCommand& pc, Result& res)
{
	if (!checkArguments(pc, res.text))
	{
		res.what = RESULT_ERROR_MESSAGE;
		return true;
	}
	switch (pc.command)
	{
		case COMMAND_OPEN_LOG:
			res.what = RESULT_NO_TEXT;
			if (pc.args.size() >= 2)
			{
				const string& type = pc.args[1];
				if (stricmp(type.c_str(), "system") == 0)
					openLogFile(LogManager::SYSTEM, res);
				else if (stricmp(type.c_str(), "downloads") == 0)
					openLogFile(LogManager::DOWNLOAD, res);
				else if (stricmp(type.c_str(), "uploads") == 0)
					openLogFile(LogManager::UPLOAD, res);
				else
				{
					res.text = STRING(COMMAND_UNKNOWN_LOG_FILE_TYPE);
					res.what = RESULT_ERROR_MESSAGE;
				}
			}
			else
			{
				res.text = STRING(COMMAND_ARG_REQUIRED);
				res.what = RESULT_ERROR_MESSAGE;
			}
			return true;

		case COMMAND_SEARCH:
			res.what = RESULT_NO_TEXT;
			SearchFrame::openWindow(Text::toT(pc.args[1]));
			return true;

		case COMMAND_MEDIA_PLAYER:
			if (pc.args[0] == "wmp")
			{
				string spam = Players::getWMPSpam(FindWindow(_T("WMPlayerApp"), NULL), WinUtil::g_mainWnd);
				checkPlayerMessage(spam, res, ResourceManager::WMP_NOT_PLAY, ResourceManager::WMP_NOT_RUN);
				return true;
			}
			if (pc.args[0] == "itunes")
			{
				string spam = Players::getItunesSpam(FindWindow(_T("iTunes"), _T("iTunes")));
				checkPlayerMessage(spam, res, ResourceManager::ITUNES_NOT_PLAY, ResourceManager::ITUNES_NOT_RUN);
				return true;
			}
			if (pc.args[0] == "mpc")
			{
				string spam = Players::getMPCSpam();
				checkPlayerMessage(spam, res, ResourceManager::MPC_NOT_RUN, ResourceManager::MPC_NOT_RUN);
				return true;
			}
			if (pc.args[0] == "ja")
			{
				string spam = Players::getJASpam();
				checkPlayerMessage(spam, res, ResourceManager::JA_NOT_RUN, ResourceManager::JA_NOT_RUN);
				return true;
			}
			if (pc.args[0] == "winamp" || pc.args[0] == "w" || pc.args[0] == "foobar" || pc.args[0] == "qcd")
			{
				string spam = Players::getWinampSpam(FindWindow(_T("PlayerCanvas"), NULL), 1);
				if (spam.empty())
				{
					HWND hwnd = FindWindow(_T("Winamp v1.x"), NULL);
					if (hwnd) spam = Players::getWinampSpam(hwnd, 0);
				}
				if (spam.empty())
				{
					if (pc.args[0] == "foobar")
						res.text = STRING(FOOBAR_ERROR);
					else if (pc.args[0] == "qcd")
						res.text = STRING(QCDQMP_NOT_RUNNING);
					else
						res.text = STRING(WINAMP_NOT_RUNNING);
					res.what = RESULT_ERROR_MESSAGE;
				}
				else
				{
					res.text = std::move(spam);
					res.what = RESULT_TEXT;
				}
				return true;
			}
			break;

		case COMMAND_AWAY:
			if (Util::getAway() && pc.args.size() < 2)
			{
				Util::setAway(false);
				setAwayButton(false);
				res.text = STRING(AWAY_MODE_OFF);
			}
			else
			{
				Util::setAway(true);
				setAwayButton(true);
				Util::setAwayMessage(pc.args.size() >= 2 ? pc.args[1] : Util::emptyString);

				StringMap sm;
				sm["userNI"] = "<username>";
				res.text = STRING(AWAY_MODE_ON) + ' ' + Util::getAwayMessage(Util::emptyString, sm);
			}
			res.what = RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_OPEN_URL:
		{
			res.what = RESULT_NO_TEXT;
			string link = pc.args[1];
			if (strnicmp("dchub://", link.c_str(), 8))
			{
				Util::ParsedUrl url;
				Util::decodeUrl(link, url, Util::emptyString);
				if (url.protocol.empty()) link.insert(0, "http://");
			}
			WinUtil::openLink(Text::toT(link));
			return true;
		}

		case COMMAND_SHUTDOWN:
		{
			bool state = !isShutDown();
			setShutDown(state);
			res.text = state ? STRING(SHUTDOWN_ON) : STRING(SHUTDOWN_OFF);
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
		case COMMAND_WEB_SEARCH:
		case COMMAND_WHOIS:
		{
			const string& url = getFirstWebSearchUrl(pc.command == COMMAND_WEB_SEARCH ? SearchUrl::KEYWORD : SearchUrl::HOSTNAME);
			if (url.empty())
			{
				res.text = STRING(COMMAND_NO_SEARCH_URL);
				res.what = RESULT_ERROR_MESSAGE;
			}
			else
			{
				InternetSearchBaseHandler::performWebSearch(url, pc.args[1]);
				res.what = RESULT_NO_TEXT;
			}
			return true;
		}
#ifdef DEBUG_GDI_IMAGE
		case COMMAND_DEBUG_GDI_INFO:
			res.what = RESULT_LOCAL_TEXT;
			res.text = "CGDIImage instance count: " + Util::toString(CGDIImage::getImageCount());
#ifdef _DEBUG
			if (pc.args.size() >= 2 && stricmp(pc.args[1].c_str(), "list") == 0)
			{
				tstring list = CGDIImage::getLoadedList();
				if (!list.empty())
					res.text += '\n' + Text::fromT(list);
			}
#endif
			return true;
#endif
	}
	return false;
}

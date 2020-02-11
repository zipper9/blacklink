#include "stdafx.h"
#include "Commands.h"
#include "../client/CFlylinkDBManager.h"
#include "../client/ClientManager.h"
#include "../client/CompatibilityManager.h"
#include "../client/ShareManager.h"
#include "Players.h"
#include "MainFrm.h"

// FIXME: Synchronize help text with actual commands.
//  Add description for /whois

tstring Commands::help()
{
	return _T("*** ") + TSTRING(CMD_FIRST_LINE) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/away, /a (message) \t\t\t") + TSTRING(CMD_AWAY_MSG) +
	       _T("\n/clear, /c \t\t\t\t") + TSTRING(CMD_CLEAR_CHAT) +
	       _T("\n/favshowjoins, /fsj \t\t\t") + TSTRING(CMD_FAV_JOINS) +
	       _T("\n/showjoins, /sj \t\t\t\t") + TSTRING(CMD_SHOW_JOINS) +
	       _T("\n/ts \t\t\t\t\t") + TSTRING(CMD_TIME_STAMP) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/slots, /sl # \t\t\t\t") + TSTRING(CMD_SLOTS) +
	       _T("\n/extraslots, /es # \t\t\t") + TSTRING(CMD_EXTRA_SLOTS) +
	       _T("\n/smallfilesize, /sfs # \t\t\t") + TSTRING(CMD_SMALL_FILES) +
	       _T("\n/refresh \t\t\t\t") + TSTRING(CMD_SHARE_REFRESH) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/join, /j # \t\t\t\t") + TSTRING(CMD_JOIN_HUB) +
	       _T("\n/close \t\t\t\t\t") + TSTRING(CMD_CLOSE_WND) +
	       _T("\n/favorite, /fav \t\t\t\t") + TSTRING(CMD_FAV_HUB) +
	       _T("\n/rmfavorite, /rf \t\t\t\t") + TSTRING(CMD_RM_FAV) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/userlist, /ul \t\t\t\t") + TSTRING(CMD_USERLIST) +
	       _T("\n/switch \t\t\t\t\t") + TSTRING(CMD_SWITCHPANELS) +
	       _T("\n/ignorelist, /il \t\t\t\t") + TSTRING(CMD_IGNORELIST) +
#if 0 // FIXME: not implemented???
	       _T("\n/favuser, /fu # \t\t\t\t") + TSTRING(CMD_FAV_USER) +
#endif
	       _T("\n/pm (user) (message) \t\t\t") + TSTRING(CMD_SEND_PM) +
	       _T("\n/getlist, /gl (user) \t\t\t") + TSTRING(CMD_GETLIST) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/flylinkdc++, /fv \t\t\t") + TSTRING(CMD_FLYLINKDC) +
	       _T("\n/uptime, /ut \t\t\t\t") + TSTRING(CMD_UPTIME) +
	       _T("\n/connection, /con \t\t\t") + TSTRING(CMD_CONNECTION) +
	       _T("\n/connection pub, /con pub\t\t\t") + TSTRING(CMD_PUBLIC_CONNECTION) +
	       // AirDC++
	       _T("\n/speed, /speed pub \t\t\t") + TSTRING(AVERAGE_DOWNLOAD_UPLOAD) +
	       _T("\n/dsp, /dsp pub \t\t\t\t") + TSTRING(DISK_SPACE) +
	       _T("\n/disks, /disks pub \t\t\t\t") + TSTRING(DISKS_INFO) +
	       _T("\n/cpu, /cpu pub \t\t\t\t") + TSTRING(CPU_INFO) +
	       // AirDC++
	       _T("\n/stats \t\t\t\t\t") + TSTRING(CMD_STATS) +
	       _T("\n/stats pub\t\t\t\t") + TSTRING(CMD_PUBLIC_STATS) +
	       _T("\n/systeminfo, /sysinfo \t\t\t") + TSTRING(CMD_SYSTEM_INFO) +
	       _T("\n/systeminfo pub, /sysinfo pub\t\t") + TSTRING(CMD_PUBLIC_SYSTEM_INFO) +
	       _T("\n/u (url) \t\t\t\t\t") + TSTRING(CMD_URL) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/search, /s (string) \t\t\t") + TSTRING(CMD_DO_SEARCH) +
	       _T("\n/google, /g (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_GOOGLE) +
	       _T("\n/define (string) \t\t\t\t") + TSTRING(CMD_DO_SEARCH_GOOGLEDEFINE) +
	       _T("\n/yandex, /y (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_YANDEX) +
	       _T("\n/yahoo, /yh (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_YAHOO) +
	       _T("\n/wikipedia, /wiki (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_WIKI) +
	       _T("\n/imdb, /i (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_IMDB) +
	       _T("\n/kinopoisk, /kp, /k (string) \t\t") + TSTRING(CMD_DO_SEARCH_KINOPOISK) +
	       _T("\n/rutracker, /rt, /t (string) \t\t") + TSTRING(CMD_DO_SEARCH_RUTRACKER) +
	       _T("\n/thepirate, /tpb (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_THEPIRATE) +
	       _T("\n/vkontakte, /vk, /v (string) \t\t") + TSTRING(CMD_DO_SEARCH_VK) +
	       _T("\n/vkid, /vid (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_VKID) +
	       _T("\n/discogs, /ds (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_DISC) +
	       _T("\n/filext, /ext (string) \t\t\t") + TSTRING(CMD_DO_SEARCH_EXT) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
	       _T("\n/savequeue, /sq \t\t\t") + TSTRING(CMD_SAVE_QUEUE) +
	       _T("\n/shutdown \t\t\t\t") + TSTRING(CMD_SHUTDOWN) +
	       _T("\n/me \t\t\t\t\t") + TSTRING(CMD_ME) +
	       _T("\n/winamp, /w (/wmp, /itunes, /mpc, /ja) \t") + TSTRING(CMD_WINAMP) +
	       // AirDC++
	       //   _T("\n/spotify, /s \t\t\t") + TSTRING(CMD_SPOTIFY) +
	       _T("\n/n \t\t\t\t\t") + TSTRING(CMD_REPLACE_WITH_LAST_INSERTED_NICK) + // SSA_SAVE_LAST_NICK_MACROS
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------\n") +
	       TSTRING(CMD_HELP_INFO) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------\n")
	       ;
}

tstring Commands::helpForCEdit()
{
	tstring text = help();
	tstring out;
	tstring::size_type pos = 0;
	for (;;)
	{
		tstring::size_type next = text.find(_T('\n'), pos);
		if (next == tstring::npos) break;
		tstring line = text.substr(pos, next-pos);
		if (!line.empty() && line[0] != _T('-'))
		{
			for (tstring::size_type i = 0; i < line.length(); i++)
				if (line[i] == _T('\t')) line[i] = _T('.');
			out += line;
		}
		out += _T("\r\n");
		pos = next + 1;
	}
	return out;
}

bool Commands::processCommand(tstring& cmd, tstring& param, tstring& message, tstring& status, tstring& localMessage)
{
	string::size_type i = cmd.find(' ');
	if (i != string::npos)
	{
		param = cmd.substr(i + 1);
		cmd = cmd.substr(1, i - 1);
	}
	else
	{
		cmd = cmd.substr(1);
	}

	if (stricmp(cmd.c_str(), _T("help")) == 0 || stricmp(cmd.c_str(), _T("h")) == 0)
	{
		localMessage = help();
		//[+] SCALOlaz
		//AboutDlgIndex dlg;    // Сделать что-то подобное, модальное окно со списком команд, чтобы не вешало чат
		//dlg.DoModal();
	}
	else if (stricmp(cmd.c_str(), _T("stats")) == 0)
	{
		if (stricmp(param.c_str(), _T("pub")) == 0)
			message = Text::toT(CompatibilityManager::generateProgramStats());
		else
			localMessage = Text::toT(CompatibilityManager::generateProgramStats());
	}
	else if (stricmp(cmd.c_str(), _T("log")) == 0)
	{
		// FIXME: query path from LogManager
		if (stricmp(param.c_str(), _T("system")) == 0)
			WinUtil::openFile(Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + "system.log")));
		else if (stricmp(param.c_str(), _T("downloads")) == 0)
			WinUtil::openFile(Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatTime(SETTING(LOG_FILE_DOWNLOAD), time(NULL)))));
		else if (stricmp(param.c_str(), _T("uploads")) == 0)
			WinUtil::openFile(Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatTime(SETTING(LOG_FILE_UPLOAD), time(NULL)))));
		else
			return false;
	}
	else if (stricmp(cmd.c_str(), _T("refresh")) == 0)
	{
		try
		{
			ShareManager::getInstance()->refreshShare();
		}
		catch (const ShareException& e)
		{
			status = Text::toT(e.getError());
		}
	}
	else if (stricmp(cmd.c_str(), _T("makefilelist")) == 0)
	{
		ShareManager::getInstance()->generateFileList();
	}
	else if (stricmp(cmd.c_str(), _T("savequeue")) == 0 || stricmp(cmd.c_str(), _T("sq")) == 0)
	{
		QueueManager::getInstance()->saveQueue();
		status = TSTRING(QUEUE_SAVED);
	}
	else if (stricmp(cmd.c_str(), _T("ignorelist")) == 0 || stricmp(cmd.c_str(), _T("il")) == 0)
	{
		localMessage = TSTRING(IGNORED_USERS) + _T(':');
		localMessage += UserManager::getInstance()->getIgnoreListAsString();
	}
	else if (stricmp(cmd.c_str(), _T("slots")) == 0)
	{
		int j = Util::toInt(param);
		if (j > 0)
		{
			SET_SETTING(SLOTS, j);
			status = TSTRING(SLOTS_SET);
			ClientManager::infoUpdated(); // Не звать если не меняется SLOTS_SET
		}
		else
		{
			status = TSTRING(INVALID_NUMBER_OF_SLOTS);
		}
	}
	else if (stricmp(cmd.c_str(), _T("extraslots")) == 0)
	{
		int j = Util::toInt(param);
		if (j > 0)
		{
			SET_SETTING(EXTRA_SLOTS, j);
			status = TSTRING(EXTRA_SLOTS_SET);
		}
		else
		{
			status = TSTRING(INVALID_NUMBER_OF_SLOTS);
		}
	}
	else if (stricmp(cmd.c_str(), _T("smallfilesize")) == 0)
	{
		int j = Util::toInt(param);
		if (j >= 64)
		{
			SET_SETTING(MINISLOT_SIZE, j);
			status = TSTRING(SMALL_FILE_SIZE_SET);
		}
		else
		{
			status = TSTRING(INVALID_SIZE);
		}
	}
	else if (stricmp(cmd.c_str(), _T("search")) == 0)
	{
		if (!param.empty())
			SearchFrame::openWindow(param);
		else
			status = TSTRING(SPECIFY_SEARCH_STRING);
	}
	else if (stricmp(cmd.c_str(), _T("flylinkdc++")) == 0 || stricmp(cmd.c_str(), _T("flylinkdc")) == 0 || stricmp(cmd.c_str(), _T("fv")) == 0)
	{
		message = _T("FlylinkDC++ version message (/fv, /flylinkdc++, /flylinkdc):\r\n -- ") _T(" <FlylinkDC++ ") T_VERSIONSTRING _T(" / ") _T(DCVERSIONSTRING) _T(">");
	}
	else if (stricmp(cmd.c_str(), _T("uptime")) == 0 || stricmp(cmd.c_str(), _T("ut")) == 0)
	{
		message = Text::toT("+me Uptime: " + Util::formatTime(Util::getUpTime()) + ". System uptime: " + CompatibilityManager::getSysUptime());
	}
	else if (stricmp(cmd.c_str(), _T("systeminfo")) == 0 || stricmp(cmd.c_str(), _T("sysinfo")) == 0)
	{
		tstring tmp = _T("+me systeminfo: ") +
		              Text::toT(CompatibilityManager::generateFullSystemStatusMessage());
		if (stricmp(param.c_str(), _T("pub")) == 0)
			message = tmp;
		else
			localMessage = tmp;
	}
	// AirDC++
	else if ((stricmp(cmd.c_str(), _T("speed")) == 0))
	{
		tstring tmp = _T("My Speed: ") + Text::toT(CompatibilityManager::Speedinfo());
		if (stricmp(param.c_str(), _T("pub")) == 0)
			message = tmp;
		else
			localMessage = tmp;
	}
	else if ((stricmp(cmd.c_str(), _T("cpu")) == 0))
	{
		tstring tmp = _T("My CPU: \r\n\t-=[ ") + Text::toT(CompatibilityManager::CPUInfo()) + _T(" ]=-");
		if (stricmp(param.c_str(), _T("pub")) == 0)
			message = tmp;
		else
			localMessage = tmp;
	}
	else if ((stricmp(cmd.c_str(), _T("dsp")) == 0))
	{
		tstring tmp = _T("My Disk Space: ") + Text::toT(CompatibilityManager::DiskSpaceInfo());
		if (stricmp(param.c_str(), _T("pub")) == 0)
			message = tmp;
		else
			localMessage = tmp;
	}
	else if (stricmp(cmd.c_str(), _T("disks")) == 0 || (stricmp(cmd.c_str(), _T("di")) == 0))
	{
		tstring tmp = _T("My Disks: ") + CompatibilityManager::diskInfo();
		if (stricmp(param.c_str(), _T("pub")) == 0)
			message = tmp;
		else
			localMessage = tmp;
	}
	// AirDC++
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	else if (stricmp(cmd.c_str(), _T("ratio")) == 0 || stricmp(cmd.c_str(), _T("r")) == 0)
	{
		StringMap params;
		CFlylinkDBManager::getInstance()->load_global_ratio();
		params["ratio"] = Text::fromT(CFlylinkDBManager::getInstance()->get_ratioW());
		params["up"] = Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_upload());
		params["down"] = Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_download());
		message = Text::toT(Util::formatParams(SETTING(RATIO_MESSAGE), params, false));
	}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	else if (stricmp(cmd.c_str(), _T("limit")) == 0)
	{
		MainFrame::getMainFrame()->onLimiter();
		status = BOOLSETTING(THROTTLE_ENABLE) ? TSTRING(LIMITER_ON) : TSTRING(LIMITER_OFF);
		// WMP9+ Support
	}
	else if (stricmp(cmd.c_str(), _T("wmp")) == 0)
	{
		string spam = Players::getWMPSpam(FindWindow(_T("WMPlayerApp"), NULL), WinUtil::g_mainWnd);
		if (!spam.empty())
		{
			if (spam != "no_media")
				message = Text::toT(spam);
			else
				status = TSTRING(WMP_NOT_PLAY);
		}
		else
		{
			status = TSTRING(WMP_NOT_RUN);
		}
	}
	else if (stricmp(cmd.c_str(), _T("itunes")) == 0)
	{
		string spam = Players::getItunesSpam(FindWindow(_T("iTunes"), _T("iTunes")));
		if (!spam.empty())
		{
			if (spam != "no_media")
				message = Text::toT(spam);
			else
				status = TSTRING(ITUNES_NOT_PLAY);
		}
		else
		{
			status = TSTRING(ITUNES_NOT_RUN);
		}
	}
	else if (stricmp(cmd.c_str(), _T("mpc")) == 0)
	{
		string spam = Players::getMPCSpam();
		if (!spam.empty())
			message = Text::toT(spam);
		else
			status = TSTRING(MPC_NOT_RUN);
	}
	else if (stricmp(cmd.c_str(), _T("ja")) == 0)
	{
		string spam = Players::getJASpam();
		if (!spam.empty())
			message = Text::toT(spam);
		else
			status = TSTRING(JA_NOT_RUN);
	}
	// AirDC++
	/*
	    else if ((stricmp(cmd.c_str(), _T("spotify")) == 0) || (stricmp(cmd.c_str(), _T("s")) == 0)) {
	        string spam = Players::getSpotifySpam(FindWindow(_T("SpotifyMainWindow"), NULL));
	        if (!spam.empty()) {
	            if (spam != "no_media") {
	                message = Text::toT(spam);
	            }
	            else {
	                status = _T("You have no media playing in Spotify");
	            }
	        }
	        else {
	            status = _T("Supported version of Spotify is not running");
	        }
	    }
	*/
	else if (stricmp(cmd.c_str(), _T("away")) == 0)
	{
		if (Util::getAway() && param.empty())
		{
			Util::setAway(false);
			MainFrame::setAwayButton(false);
			status = TSTRING(AWAY_MODE_OFF);
		}
		else
		{
			Util::setAway(true);
			MainFrame::setAwayButton(true);
			Util::setAwayMessage(Text::fromT(param));
			
			StringMap sm;
			status = TSTRING(AWAY_MODE_ON) + _T(' ') + Text::toT(Util::getAwayMessage(sm));
		}
	}
	else if (stricmp(cmd.c_str(), _T("u")) == 0)
	{
		if (!param.empty())
			WinUtil::openLink(Text::toT(Util::encodeURI(Text::fromT(param))));
	}
#if 0
	else if (stricmp(cmd.c_str(), _T("rebuild")) == 0)
	{
		HashManager::getInstance()->rebuild();
	}
#endif
	else if (stricmp(cmd.c_str(), _T("shutdown")) == 0)
	{
		MainFrame::setShutDown(!(MainFrame::isShutDown()));
		if (MainFrame::isShutDown())
			status = TSTRING(SHUTDOWN_ON);
		else
			status = TSTRING(SHUTDOWN_OFF);
	}
	else if (stricmp(cmd.c_str(), _T("winamp")) == 0 || stricmp(cmd.c_str(), _T("w")) == 0 || stricmp(cmd.c_str(), _T("f")) == 0 || stricmp(cmd.c_str(), _T("foobar")) == 0 || stricmp(cmd.c_str(), _T("qcd")) == 0 || stricmp(cmd.c_str(), _T("q")) == 0)
	{
		string spam = Players::getWinampSpam(FindWindow(_T("PlayerCanvas"), NULL), 1);
		if (!spam.empty())
		{
			message = Text::toT(spam);
		}
		else
		{
			if (FindWindow(_T("Winamp v1.x"), NULL))
			{
				spam = Players::getWinampSpam(FindWindow(_T("Winamp v1.x"), NULL), 0);
				if (!spam.empty()) message = Text::toT(spam);
			}
			else if (stricmp(cmd.c_str(), _T("f")) == 0 || stricmp(cmd.c_str(), _T("foobar")) == 0)
				status = TSTRING(FOOBAR_ERROR);
			else if (stricmp(cmd.c_str(), _T("qcd")) == 0 || stricmp(cmd.c_str(), _T("q")) == 0)
				status = TSTRING(QCDQMP_NOT_RUNNING);
			else
				status = TSTRING(WINAMP_NOT_RUNNING);
		}
	}
#ifdef IRAINMAN_ENABLE_MORE_CLIENT_COMMAND
	// Google.
	else if (stricmp(cmd.c_str(), _T("google")) == 0 || stricmp(cmd.c_str(), _T("g")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://www.google.com/search?q=") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// Google defination search support.
	else if (stricmp(cmd.c_str(), _T("define")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://www.google.com/search?hl=en&q=define%3A+") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// Yandex.
	else if (stricmp(cmd.c_str(), _T("yandex")) == 0 || stricmp(cmd.c_str(), _T("y")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://yandex.ru/yandsearch?text=") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// Yahoo.
	else if (stricmp(cmd.c_str(), _T("yahoo")) == 0 || stricmp(cmd.c_str(), _T("yh")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://search.yahoo.com/search?p=") + Text::toT(Util::encodeURI(Text::fromT(param))));
		
	}
	// Wikipedia.
	else if (stricmp(cmd.c_str(), _T("wikipedia")) == 0 || stricmp(cmd.c_str(), _T("wiki")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://ru.wikipedia.org/wiki/Special%3ASearch?search=") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// IMDB.
	else if (stricmp(cmd.c_str(), _T("imdb")) == 0 || stricmp(cmd.c_str(), _T("i")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://www.imdb.com/find?q=") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// КиноПоиск.Ru.
	else if (stricmp(cmd.c_str(), _T("kinopoisk")) == 0 || stricmp(cmd.c_str(), _T("kp")) == 0 || stricmp(cmd.c_str(), _T("k")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://www.kinopoisk.ru/index.php?first=no&kp_query=") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// TPB
	else if (stricmp(cmd.c_str(), _T("thepirate")) == 0 || stricmp(cmd.c_str(), _T("tpb")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://thepiratebay.se/search/") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// Rutracker.org.
	else if (stricmp(cmd.c_str(), _T("rutracker")) == 0 || stricmp(cmd.c_str(), _T("rt")) == 0 || stricmp(cmd.c_str(), _T("t")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://rutracker.org/forum/tracker.php?nm=") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// В Контакте.
	else if (stricmp(cmd.c_str(), _T("vkontakte")) == 0 || stricmp(cmd.c_str(), _T("vk")) == 0 || stricmp(cmd.c_str(), _T("v")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://vk.com/gsearch.php?q=") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// В Контакте. Открываем страницу по id.
	else if (stricmp(cmd.c_str(), _T("vkid")) == 0 || stricmp(cmd.c_str(), _T("vid")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://vk.com/") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
	// Discogs is a user-built database containing information on artists, labels, and their recordings.
	else if (stricmp(cmd.c_str(), _T("discogs")) == 0 || stricmp(cmd.c_str(), _T("ds")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://www.discogs.com/search?type=all&q=") + Text::toT(Util::encodeURI(Text::fromT(param))) + _T("&btn=Search"));
	}
	// FILExt. To find a description of the file extension / Для поиска описания расширения файла.
	else if (stricmp(cmd.c_str(), _T("filext")) == 0 || stricmp(cmd.c_str(), _T("ext")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://filext.com/file-extension/") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
#endif // IRAINMAN_ENABLE_MORE_CLIENT_COMMAND
#ifdef IRAINMAN_ENABLE_WHOIS
	else if (stricmp(cmd.c_str(), _T("whois")) == 0)
	{
		if (param.empty())
			status = TSTRING(SPECIFY_SEARCH_STRING);
		else
			WinUtil::openLink(_T("http://www.ripe.net/perl/whois?form_type=simple&full_query_string=&searchtext=") + Text::toT(Util::encodeURI(Text::fromT(param))));
	}
#endif
#ifdef TEST_CRASH_HANDLER
	else if (stricmp(cmd.c_str(), _T("divide")) == 0)
	{
		int a = Util::toInt(param);
		int b = 1;
		string::size_type pos = param.find(' ');
		if (pos != string::npos)
			b = Util::toInt(param.substr(pos+1));
		int result = a/b;
		localMessage = _T("Your answer is ") + Util::toStringW(result);
	}
#endif
	else return false;
	
	return true;
}

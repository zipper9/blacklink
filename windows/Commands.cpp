#include "stdafx.h"
#include "Commands.h"
#include "../client/DatabaseManager.h"
#include "../client/ClientManager.h"
#include "../client/ConnectionManager.h"
#include "../client/UploadManager.h"
#include "../client/CompatibilityManager.h"
#include "../client/ShareManager.h"
#include "../client/LocationUtil.h"
#include "../client/ParamExpander.h"
#include "../client/dht/DHT.h"
#include "../client/dht/DHTSearchManager.h"
#include "../client/dht/IndexManager.h"
#include "Players.h"
#include "MainFrm.h"

#ifdef _DEBUG
extern bool suppressUserConn;
#endif

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
	       _T("\n/extraslots      # \t\t\t") + TSTRING(CMD_EXTRA_SLOTS) +
	       _T("\n/smallfilesize       # \t\t\t") + TSTRING(CMD_SMALL_FILES) +
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
	       _T("\n/version, /ver \t\t\t") + TSTRING(CMD_VERSION) +
	       _T("\n/uptime, /ut \t\t\t\t") + TSTRING(CMD_UPTIME) +
	       _T("\n/connection, /con \t\t\t") + TSTRING(CMD_CONNECTION) +
	       _T("\n/connection pub, /con pub\t\t\t") + TSTRING(CMD_PUBLIC_CONNECTION) +
	       _T("\n/speed, /speed pub \t\t\t") + TSTRING(AVERAGE_DOWNLOAD_UPLOAD) +
	       _T("\n/dsp, /dsp pub \t\t\t\t") + TSTRING(DISK_SPACE) +
	       _T("\n/disks, /disks pub \t\t\t\t") + TSTRING(DISKS_INFO) +
	       _T("\n/cpu, /cpu pub \t\t\t\t") + TSTRING(CPU_INFO) +
	       _T("\n/stats \t\t\t\t\t") + TSTRING(CMD_STATS) +
	       _T("\n/stats pub\t\t\t\t") + TSTRING(CMD_PUBLIC_STATS) +
	       _T("\n/systeminfo, /sysinfo \t\t\t") + TSTRING(CMD_SYSTEM_INFO) +
	       _T("\n/systeminfo pub, /sysinfo pub\t\t") + TSTRING(CMD_PUBLIC_SYSTEM_INFO) +
	       _T("\n/u (url) \t\t\t\t\t") + TSTRING(CMD_URL) +
	       _T("\n------------------------------------------------------------------------------------------------------------------------------------------------------------") +
#ifdef IRAINMAN_ENABLE_MORE_CLIENT_COMMAND
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
#endif
	       _T("\n/savequeue, /sq \t\t\t") + TSTRING(CMD_SAVE_QUEUE) +
	       _T("\n/shutdown \t\t\t\t") + TSTRING(CMD_SHUTDOWN) +
	       _T("\n/me \t\t\t\t\t") + TSTRING(CMD_ME) +
	       _T("\n/winamp, /w (/wmp, /itunes, /mpc, /ja) \t") + TSTRING(CMD_WINAMP) +
	       // AirDC++
	       //   _T("\n/spotify, /s \t\t\t") + TSTRING(CMD_SPOTIFY) +
	       _T("\n/n \t\t\t\t\t") + TSTRING(CMD_REPLACE_WITH_LAST_INSERTED_NICK) + // SSA_SAVE_LAST_NICK_MACROS
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

static void openLogFile(int area, tstring& localMessage)
{
	string filename = LogManager::getLogFileName(area, StringMap());
	if (!File::isExist(filename))
	{
		localMessage = TSTRING(COMMAND_NO_LOG_FILE);
		return;
	}
	WinUtil::openFile(Text::toT(filename));
}

static inline bool whitespace(TCHAR c)
{
	return c == _T(' ') || c == _T('\t');
}

static void splitParams(const tstring& cmd, vector<tstring>& params)
{
	for (size_t i = 0; i < cmd.length(); i++)
	{
		if (whitespace(cmd[i])) continue;
		tstring newParam;
		bool quote = false;
		for (; i < cmd.length() && (quote || !whitespace(cmd[i])); i++)
		{
			if (cmd[i] == _T('"'))
			{
				quote = !quote;
				if (quote) continue;
			}
			newParam += cmd[i];
		}
		if (!newParam.empty())
			params.emplace_back(std::move(newParam));
	}
}

static bool parseCID(CID& cid, const tstring& param, tstring& localMessage)
{
	bool error;
	if (param.length() == 39)
		Encoder::fromBase32(Text::fromT(param).c_str(), cid.writableData(), CID::SIZE, &error);
	else
		error = true;
	if (error)
	{
		localMessage = TSTRING(COMMAND_INVALID_CID);
		return false;
	}
	return true;
}

static bool parseTTH(TTHValue& tth, const tstring& param, tstring& localMessage)
{
	bool error;
	if (param.length() == 39)
		Encoder::fromBase32(Text::fromT(param).c_str(), tth.data, TTHValue::BYTES, &error);
	else
		error = true;
	if (error)
	{
		localMessage = TSTRING(INVALID_TTH);
		return false;
	}
	return true;
}

bool Commands::processCommand(tstring& cmd, tstring& param, tstring& message, tstring& status, tstring& localMessage)
{
	tstring::size_type i = cmd.find(' ');
	if (i != string::npos)
	{
		param = cmd.substr(i + 1);
		cmd = cmd.substr(1, i - 1);
		boost::algorithm::trim(param);
	}
	else
	{
		cmd = cmd.substr(1);
	}

	boost::algorithm::trim(cmd);

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
		if (param.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		if (stricmp(param.c_str(), _T("system")) == 0)
			openLogFile(LogManager::SYSTEM, localMessage);
		else if (stricmp(param.c_str(), _T("downloads")) == 0)
			openLogFile(LogManager::DOWNLOAD, localMessage);
		else if (stricmp(param.c_str(), _T("uploads")) == 0)
			openLogFile(LogManager::UPLOAD, localMessage);
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
		localMessage = TSTRING(COMMAND_DONE);
	}
	else if (stricmp(cmd.c_str(), _T("sharefile")) == 0)
	{
		if (param.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		string path = Text::fromT(param);
		string dir = Util::getFilePath(path);
		if (!ShareManager::getInstance()->isDirectoryShared(dir))
		{
			localMessage = TSTRING(DIRECTORY_NOT_SHARED);
			return true;
		}
		TigerTree tree;
		std::atomic_bool stopFlag(false);
		if (!Util::getTTH(path, true, 512 * 1024, stopFlag, tree))
		{
			localMessage = TSTRING(COMMAND_TTH_ERROR);
			return true;
		}
		try
		{
			ShareManager::getInstance()->addFile(path, tree.getRoot());
			DatabaseManager::getInstance()->addTree(tree);
			localMessage = TSTRING_F(COMMAND_FILE_SHARED,
				Text::toT(Util::getMagnet(tree.getRoot(), Util::getFileName(path), tree.getFileSize())));
		}
		catch (Exception& e)
		{
			localMessage = Text::toT(e.getError());
		}
	}
#ifdef _DEBUG
	else if (stricmp(cmd.c_str(), _T("addtree")) == 0)
	{
		if (param.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		TigerTree tree;
		std::atomic_bool stopFlag(false);
		if (!Util::getTTH(Text::fromT(param), true, 512 * 1024, stopFlag, tree))
		{
			localMessage = TSTRING(COMMAND_TTH_ERROR);
			return true;
		}
		if (DatabaseManager::getInstance()->addTree(tree))
			localMessage = TSTRING_F(COMMAND_TTH_ADDED, Text::toT(tree.getRoot().toBase32()));
		else
			localMessage = _T("Unable to add tree");
	}
#endif
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
	else if (stricmp(cmd.c_str(), _T("slots")) == 0 || stricmp(cmd.c_str(), _T("sl")) == 0)
	{
		int j = Util::toInt(param);
		if (j > 0)
		{
			if (j != SETTING(SLOTS))
			{
				SET_SETTING(SLOTS, j);
				ClientManager::infoUpdated();
			}
			status = TSTRING(SLOTS_SET);
		}
		else
			status = TSTRING(INVALID_NUMBER_OF_SLOTS);
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
			status = TSTRING(INVALID_NUMBER_OF_SLOTS);
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
			status = TSTRING(INVALID_SIZE);
	}
	else if (stricmp(cmd.c_str(), _T("search")) == 0)
	{
		if (!param.empty())
			SearchFrame::openWindow(param);
		else
			status = TSTRING(SPECIFY_SEARCH_STRING);
	}
	else if (stricmp(cmd.c_str(), _T("version")) == 0 || stricmp(cmd.c_str(), _T("ver")) == 0)
	{
		message = _T(APPNAME " " VERSION_STR " / " DCVERSIONSTRING);
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
		tstring tmp = _T("My CPU: ") + Text::toT(CompatibilityManager::CPUInfo());
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
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	else if (stricmp(cmd.c_str(), _T("ratio")) == 0 || stricmp(cmd.c_str(), _T("r")) == 0)
	{
		StringMap params;
		auto dm = DatabaseManager::getInstance();
		dm->loadGlobalRatio();
		const DatabaseManager::GlobalRatio& ratio = dm->getGlobalRatio();
		double r = ratio.download > 0 ? (double) ratio.upload / (double) ratio.download : 0;
		params["ratio"] = Util::toString(r);
		params["up"] = Util::formatBytes(ratio.upload);
		params["down"] = Util::formatBytes(ratio.download);
		message = Text::toT(Util::formatParams(SETTING(RATIO_MESSAGE), params, false));
	}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	else if (stricmp(cmd.c_str(), _T("limit")) == 0)
	{
		MainFrame::onLimiter();
		status = BOOLSETTING(THROTTLE_ENABLE) ? TSTRING(LIMITER_ON) : TSTRING(LIMITER_OFF);
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
			status = TSTRING(AWAY_MODE_ON) + _T(' ') + Text::toT(Util::getAwayMessage(Util::emptyString, sm));
		}
	}
	else if (stricmp(cmd.c_str(), _T("u")) == 0)
	{
		if (!param.empty())
			WinUtil::openLink(Text::toT(Util::encodeURI(Text::fromT(param))));
		else
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
	}
	else if (stricmp(cmd.c_str(), _T("shutdown")) == 0)
	{
		auto mainFrame = MainFrame::getMainFrame();
		bool state = !mainFrame->isShutDown();
		mainFrame->setShutDown(state);
		if (state)
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
			HWND hwnd = FindWindow(_T("Winamp v1.x"), NULL);
			if (hwnd)
			{
				spam = Players::getWinampSpam(hwnd, 0);
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
	else if (stricmp(cmd.c_str(), _T("geoip")) == 0)
	{
		if (param.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		uint32_t addr;
		if (!Util::parseIpAddress(addr, param))
		{
			localMessage = TSTRING(COMMAND_INVALID_IP);
			return true;
		}
		IPInfo ipInfo;
		Util::getIpInfo(addr, ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
		if (!ipInfo.country.empty() || !ipInfo.location.empty())
		{
			localMessage = TSTRING(LOCATION_BARE) + _T(": ");
			if (!ipInfo.country.empty() && !ipInfo.location.empty())
			{
				localMessage += Text::toT(ipInfo.country);
				localMessage += _T(", ");
			}
			localMessage += Text::toT(Util::getDescription(ipInfo));
		}
		else
			localMessage = _T("Location not found");
	}
	else if (stricmp(cmd.c_str(), _T("pginfo")) == 0)
	{
		if (param.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		uint32_t addr;
		if (!Util::parseIpAddress(addr, param))
		{
			localMessage = TSTRING(COMMAND_INVALID_IP);
			return true;
		}
		IPInfo ipInfo;
		Util::getIpInfo(addr, ipInfo, IPInfo::FLAG_P2P_GUARD);
		if (!ipInfo.p2pGuard.empty())
			localMessage += param + _T(": ") + Text::toT(ipInfo.p2pGuard);
		else
			localMessage = _T("IP not found");
	}
	else if (stricmp(cmd.c_str(), _T("tth")) == 0)
	{
		vector<tstring> args;
		splitParams(param, args);
		if (args.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		Text::makeLower(args[0]);
		if (args[0] == _T("info"))
		{
			if (args.size() < 2)
			{
				localMessage = TSTRING_F(COMMAND_N_ARGS_REQUIRED, 2);
				return true;
			}
			TTHValue tth;
			if (!parseTTH(tth, args[1], localMessage)) return true;
			string path;
			size_t treeSize;
			unsigned flags;
			tstring tthText = _T("TTH ") + args[1] + _T(": ");
			tstring result = tthText;
			if (!DatabaseManager::getInstance()->getFileInfo(tth, flags, &path, &treeSize))
			{
				result += _T("not found in database\n");
			}
			else
			{
				result += _T("found in database, flags=") + Util::toStringT(flags);
				if (!path.empty())
				{
					result += _T(", path=");
					result += Text::toT(path);
				}
				if (treeSize)
				{
					result += _T(", treeSize=");
					result += Util::toStringT(treeSize);
				}
				result += _T('\n');
			}
			result += tthText;
			int64_t size;
			if (!ShareManager::getInstance()->getFileInfo(tth, path, size))
				result += _T("not found in share\n");
			else
				result += _T("found in share, path=") + Text::toT(path);
			localMessage = std::move(result);
			return true;
		}
		if (args[0] == _T("addtree"))
		{
			if (args.size() < 3)
			{
				localMessage = TSTRING_F(COMMAND_N_ARGS_REQUIRED, 3);
				return true;
			}
			TigerTree tree;
			try
			{
				int64_t fileSize = Util::toInt64(args[2]);
				File f(args[1], File::READ, File::OPEN, false);
				const int64_t treeSize = f.getSize();
				if ((treeSize % TigerHash::BYTES) || treeSize < TigerHash::BYTES*2 || treeSize > 1024 * 1024)
					throw Exception("Invalid tree data");
				if (fileSize < ((treeSize / (int) TigerHash::BYTES) << 10))
					throw Exception("Invalid file size");
				ByteVector data;
				data.resize(treeSize);
				size_t len = treeSize;
				f.read(data.data(), len);
				TigerTree tree;
				tree.load(fileSize, data.data(), data.size());
				if (DatabaseManager::getInstance()->addTree(tree))
					localMessage = TSTRING_F(COMMAND_TTH_ADDED, Text::toT(tree.getRoot().toBase32()));
				else
					localMessage = _T("Unable to add tree");
			}
			catch (Exception& e)
			{
				localMessage = Text::toT(e.getError());
			}
			return true;
		}
		localMessage = TSTRING(COMMAND_INVALID_ACTION);
		return true;
	}
	else if (stricmp(cmd.c_str(), _T("uconn")) == 0)
	{
		vector<tstring> args;
		splitParams(param, args);
		if (args.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		Text::makeLower(args[0]);
		if (args[0] == _T("list"))
		{		
			string info = ConnectionManager::getInstance()->getUserConnectionInfo();
			if (info.empty())
				localMessage = _T("Empty list");
			else
				localMessage = Text::toT(info);
			return true;
		}
		if (args[0] == _T("expect"))
		{		
			string info = ConnectionManager::getInstance()->getExpectedInfo();
			if (info.empty())
				localMessage = _T("Empty list");
			else
				localMessage = Text::toT(info);
			return true;
		}
		if (args[0] == _T("tokens"))
		{
			string info = ConnectionManager::tokenManager.getInfo();
			if (info.empty())
				localMessage = _T("Empty list");
			else
				localMessage = Text::toT(info);
			return true;
		}
#ifdef _DEBUG
		if (args[0] == _T("suppress"))
		{
			suppressUserConn = !suppressUserConn;
			localMessage = _T("Suppress: ") + Util::toStringT(suppressUserConn);
			return true;
		}
#endif
		localMessage = TSTRING(COMMAND_INVALID_ACTION);
		return true;
	}
	else if (stricmp(cmd.c_str(), _T("grants")) == 0)
	{
		vector<UploadManager::ReservedSlotInfo> info;
		UploadManager::getInstance()->getReservedSlots(info);
		uint64_t currentTick = GET_TICK();
		string result;
		for (const auto& rs : info)
		{
			uint64_t seconds = rs.timeout < currentTick ? 0 : (rs.timeout-currentTick)/1000;
			if (!result.empty()) result += '\n';
			result += rs.user->getLastNick() + '/' + rs.user->getCID().toBase32();
			result += " timeout: " + Util::toString(seconds);
		}
		if (result.empty())
			localMessage = TSTRING(COMMAND_EMPTY_LIST);
		else
			localMessage = Text::toT(result);
		return true;
	}
	else if (stricmp(cmd.c_str(), _T("dht")) == 0)
	{
		vector<tstring> args;
		splitParams(param, args);
		if (args.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		Text::makeLower(args[0]);
		if (args[0] == _T("info"))
		{
			dht::DHT* d = dht::DHT::getInstance();
			string result = "DHT port: " + Util::toString(d->getPort()) + '\n';
			string externalIp;
			bool isFirewalled;
			d->getPublicIPInfo(externalIp, isFirewalled);
			result += "External IP: " + externalIp;
			result += isFirewalled ? " (firewalled)" : " (open)";
			result += "\nConnected: ";
			result += d->isConnected() ? "yes" : "no";
			result += "\nState: ";
			result += Util::toString(d->getState());
			result += '\n';
			size_t nodeCount = 0;
			{
				dht::DHT::LockInstanceNodes lock(d);
				const auto nodes = lock.getNodes();
				if (nodes) nodeCount = nodes->size();
			}
			result += "Nodes: " + Util::toString(nodeCount) + '\n';
			localMessage = Text::toT(result);
			return true;
		}
		if (args[0] == _T("nodes"))
		{
			unsigned maxType = 4;
			if (args.size() >= 2) maxType = Util::toInt(args[1]);
			dht::DHT* d = dht::DHT::getInstance();
			vector<dht::Node::Ptr> nv;
			{
				dht::DHT::LockInstanceNodes lock(d);
				const auto nodes = lock.getNodes();
				if (nodes)
				{
					for (const auto& node : *nodes)
						if (node->getType() <= maxType) nv.push_back(node);
				}
			}
			std::sort(nv.begin(), nv.end(), [](const dht::Node::Ptr& n1, const dht::Node::Ptr& n2) { return n1->getUser()->getCID() < n2->getUser()->getCID(); });
			uint64_t now = GET_TICK();
			string result = "Nodes: " + Util::toString(nv.size()) + '\n';
			for (const auto& node : nv)
			{
				const UserPtr& user = node->getUser();
				result += user->getCID().toBase32();
				result += ": ";
				const Identity& id = node->getIdentity();
				string nick = id.getNick();
				result += nick.empty() ? "<empty>" : nick;
				result += ' ';
				result += id.getIpAsString();
				result += ':';
				result += Util::toString(id.getUdpPort());
				uint64_t expires = node->getExpires();
				if (expires)
				{
					int seconds = 0;
					if (expires > now) seconds = static_cast<int>((expires - now) / 1000);
					result += " Expires=" + Util::toString(seconds);
				}
				result += " Type=" + Util::toString(node->getType());
				if (node->isIpVerified()) result += " Verified";
				result += '\n';
			}
			localMessage = Text::toT(result);
			return true;
		}
		if (args[0] == _T("find"))
		{
			if (args.size() < 2)
			{
				localMessage = TSTRING_F(COMMAND_N_ARGS_REQUIRED, 2);
				return true;
			}
			TTHValue tth;
			if (!parseTTH(tth, args[1], localMessage)) return true;
			dht::DHT::getInstance()->findFile(tth.toBase32(), Util::rand(), nullptr);
			localMessage = _T("DHT: file search started");
			return true;
		}
		if (args[0] == _T("fnode"))
		{
			if (args.size() < 2)
			{
				localMessage = TSTRING_F(COMMAND_N_ARGS_REQUIRED, 2);
				return true;
			}
			CID cid;
			if (!parseCID(cid, args[1], localMessage)) return true;
			dht::SearchManager::getInstance()->findNode(cid);
			localMessage = _T("DHT: node search started");
			return true;
		}
		if (args[0] == _T("ping"))
		{
			if (args.size() < 2)
			{
				localMessage = TSTRING_F(COMMAND_N_ARGS_REQUIRED, 2);
				return true;
			}
			CID cid;
			if (!parseCID(cid, args[1], localMessage)) return true;
			if (dht::DHT::getInstance()->pingNode(cid))
				localMessage = _T("DHT: pinging node");
			else
				localMessage = _T("Node not found");
			return true;
		}
		if (args[0] == _T("publish"))
		{
			if (args.size() < 2)
			{
				localMessage = TSTRING_F(COMMAND_N_ARGS_REQUIRED, 2);
				return true;
			}
			TTHValue tth;
			if (!parseTTH(tth, args[1], localMessage)) return true;
			string filename;
			int64_t size;
			if (!ShareManager::getInstance()->getFileInfo(tth, filename, size))
			{
				localMessage = _T("File not found");
				return true;
			}
			auto im = dht::IndexManager::getInstance();
			if (im)
			{
				im->publishFile(tth, size);
				string result = "Publishing file " + filename + " (";
				result += Util::toString(size);
				result += ')';
				localMessage = Text::toT(result);
			}
			else
				localMessage = _T("Could not publish this file");
			return true;
		}
		localMessage = TSTRING(COMMAND_INVALID_ACTION);
		return true;
	}
	else if (stricmp(cmd.c_str(), _T("user")) == 0)
	{
		vector<tstring> args;
		splitParams(param, args);
		if (args.size() < 2)
		{
			localMessage = TSTRING_F(COMMAND_N_ARGS_REQUIRED, 2);
			return true;
		}
		enum { ACTION_SHOW_INFO, ACTION_GET_LIST };
		int action;
		Text::makeLower(args[0]);
		if (args[0] == _T("getlist"))
			action = ACTION_GET_LIST;
		else if (args[0] == _T("info"))
			action = ACTION_SHOW_INFO;
		else
		{
			localMessage = TSTRING(COMMAND_INVALID_ACTION);
			return true;
		}
		bool hasCID = false;
		string hubUrl;
		CID cid;
		if (parseCID(cid, args[1], localMessage)) hasCID = true;
		if (args.size() >= 3)
		{
			hubUrl = Text::fromT(args[2]);
		}
		else if (!hasCID)
		{
			localMessage = _T("Hub URL must be specified");
			return true;
		}
		if (!hasCID)
			cid = ClientManager::makeCid(Text::fromT(args[1]), hubUrl);
		OnlineUserPtr ou = ClientManager::findOnlineUser(cid, hubUrl, !hubUrl.empty());
		if (!ou)
		{
			localMessage = _T("User not found");
			return true;
		}
		if (action == ACTION_GET_LIST)
		{
			ou->getList();
			localMessage = _T("Getting file list");
		}
		else
		{
			string report;
			ou->getIdentity().getReport(report);
			localMessage = Text::toT(report);
		}
		return true;
	}
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	else if (stricmp(cmd.c_str(), _T("flushdb")) == 0)
	{
		ClientManager::flushRatio();
		localMessage = _T("Stats flushed");
		return true;
	}
#endif
#ifdef _DEBUG
	else if (stricmp(cmd.c_str(), _T("bloom")) == 0)
	{
		vector<tstring> args;
		splitParams(param, args);
		if (args.empty())
		{
			localMessage = TSTRING(COMMAND_ARG_REQUIRED);
			return true;
		}
		if (args[0] == _T("info"))
		{
			size_t size, used;
			ShareManager::getInstance()->getBloomInfo(size, used);
			localMessage = _T("Size: ") + Util::toStringT(size) + _T(", used: ") + Util::toStringT(used)
				+ _T(" (") + Util::toStringT((double) used * 100.0 / (double) size) + _T("%)");
			return true;
		}
		if (args[0] == _T("match"))
		{
			if (args.size() < 2)
			{
				localMessage = TSTRING_F(COMMAND_N_ARGS_REQUIRED, 2);
				return true;
			}
			tstring textLower = Text::toLower(args[1]);
			localMessage = textLower;
			localMessage += ShareManager::getInstance()->matchBloom(Text::fromT(textLower)) ?
				_T(": Match found") : _T(": No match");
			return true;
		}
		return true;
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
		localMessage = _T("Your answer is ") + Util::toStringT(result);
	}
#endif
	else return false;
	
	return true;
}

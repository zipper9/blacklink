#include "stdinc.h"
#include "Commands.h"
#include "ResourceManager.h"
#include "DatabaseManager.h"
#include "ClientManager.h"
#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "QueueManager.h"
#include "UserManager.h"
#include "UploadManager.h"
#include "SettingsManager.h"
#include "PathUtil.h"
#include "HashUtil.h"
#include "FormatUtil.h"
#include "Util.h"
#include "ParamExpander.h"
#include "HttpClient.h"
#include "IpTest.h"
#include "AntiFlood.h"
#include "AdcSupports.h"
#include "Random.h"
#include "AppPaths.h"
#include "ConfCore.h"
#include "AppStats.h"
#include "SysInfo.h"
#include "dht/DHT.h"
#include "dht/DHTSearchManager.h"
#include "dht/IndexManager.h"
#include "version.h"

#ifdef _WIN32
#include "CompatibilityManager.h"
#endif

#ifdef _DEBUG
extern bool suppressUserConn;
extern bool disablePartialListUploads;
#endif

extern IpBans udpBans;
extern IpBans tcpBans;

using namespace Commands;

static const unsigned CTX_GENERAL_CHAT = CTX_HUB | CTX_USER | FLAG_GENERAL_CHAT;

static const CommandDescription desc[] =
{
	{ CTX_GENERAL_CHAT | FLAG_UI,                       0, 0,        ResourceManager::CMD_HELP_CLEAR               }, // COMMAND_CLEAR
	{ CTX_GENERAL_CHAT | FLAG_UI,                       0, 1,        ResourceManager::CMD_HELP_FIND_TEXT           }, // COMMAND_FIND_TEXT
	{ CTX_GENERAL_CHAT | FLAG_UI,                       0, 0,        ResourceManager::CMD_HELP_CLOSE               }, // COMMAND_CLOSE
	{ CTX_HUB | FLAG_UI,                                1, 1,        ResourceManager::CMD_HELP_JOIN                }, // COMMAND_JOIN
	{ CTX_HUB | CTX_USER,                               0, 0,        ResourceManager::CMD_HELP_ADD_FAVORITE        }, // COMMAND_ADD_FAVORITE
	{ CTX_HUB | CTX_USER,                               0, 0,        ResourceManager::CMD_HELP_REMOVE_FAVORITE     }, // COMMAND_REMOVE_FAVORITE
	{ CTX_HUB,                                          0, 0,        ResourceManager::CMD_HELP_SHOW_JOINS          }, // COMMAND_SHOW_JOINS
	{ CTX_HUB,                                          0, 0,        ResourceManager::CMD_HELP_FAV_SHOW_JOINS      }, // COMMAND_FAV_SHOW_JOINS
	{ CTX_GENERAL_CHAT,                                 0, 0,        ResourceManager::CMD_HELP_TIMESTAMPS          }, // COMMAND_TIMESTAMPS
	{ CTX_HUB,                                          0, 1,        ResourceManager::CMD_HELP_INFO_CONNECTION     }, // COMMAND_INFO_CONNECTION
	{ CTX_SYSTEM,                                       0, 1,        ResourceManager::CMD_HELP_AWAY                }, // COMMAND_AWAY
	{ CTX_SYSTEM,                                       0, 0,        ResourceManager::CMD_HELP_LIMIT               }, // COMMAND_LIMIT
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        ResourceManager::CMD_HELP_SET_SLOTS           }, // COMMAND_SET_SLOTS
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        ResourceManager::CMD_HELP_SET_EXTRA_SLOTS     }, // COMMAND_SET_EXTRA_SLOTS
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        ResourceManager::CMD_HELP_SET_SMALL_FILE_SIZE }, // COMMAND_SET_SMALL_FILE_SIZE
	{ CTX_SYSTEM,                                       0, 0,        ResourceManager::CMD_HELP_REFRESH_SHARE       }, // COMMAND_REFRESH_SHARE
	{ CTX_SYSTEM,                                       0, 0,        0                                             }, // COMMAND_MAKE_FILE_LIST
	{ CTX_SYSTEM,                                       0, 0,        0                                             }, // COMMAND_SHARE_FILE
	{ CTX_SYSTEM,                                       0, 0,        ResourceManager::CMD_HELP_SAVE_QUEUE          }, // COMMAND_SAVE_QUEUE
	{ CTX_SYSTEM,                                       0, 0,        0                                             }, // COMMAND_FLUSH_STATS
	{ CTX_HUB,                                          1, 1,        ResourceManager::CMD_HELP_PASSWORD            }, // COMMAND_PASSWORD
	{ CTX_HUB | FLAG_UI,                                0, 0,        ResourceManager::CMD_HELP_TOGGLE_USER_LIST    }, // COMMAND_TOGGLE_USER_LIST
	{ CTX_HUB | FLAG_UI,                                0, 0,        ResourceManager::CMD_HELP_USER_LIST_LOCATION  }, // COMMAND_USER_LIST_LOCATION
	{ CTX_HUB | FLAG_GET_FIRST_ARG,                     2, 2,        ResourceManager::CMD_HELP_PRIVATE_MESSAGE     }, // COMMAND_PRIVATE_MESSAGE
	{ CTX_HUB | FLAG_GET_FIRST_ARG,                     2, 2,        ResourceManager::CMD_HELP_MC_PRIVATE_MESSAGE  }, // COMMAND_MC_PRIVATE_MESSAGE
	{ CTX_HUB | CTX_USER,                               0, 1,        ResourceManager::CMD_HELP_GET_LIST            }, // COMMAND_GET_LIST
	{ CTX_USER,                                         0, 0,        ResourceManager::CMD_HELP_GRANT_EXTRA_SLOT    }, // COMMAND_GRANT_EXTRA_SLOT
	{ CTX_USER,                                         0, 0,        ResourceManager::CMD_HELP_UNGRANT_EXTRA_SLOT  }, // COMMAND_UNGRANT_EXTRA_SLOT
	{ CTX_USER,                                         0, 0,        0                                             }, // COMMAND_CCPM
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_IP_UPDATE           }, // COMMAND_IP_UPDATE
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_VERSION        }, // COMMAND_INFO_VERSION
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_UPTIME         }, // COMMAND_INFO_UPTIME
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_SPEED          }, // COMMAND_INFO_SPEED
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_STORAGE        }, // COMMAND_INFO_STORAGE
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_DISK_SPACE     }, // COMMAND_INFO_DISK_SPACE
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_SYSTEM         }, // COMMAND_INFO_SYSTEM
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_CPU            }, // COMMAND_INFO_CPU
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_STATS          }, // COMMAND_INFO_STATS
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_RATIO          }, // COMMAND_INFO_RATIO
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_SPLIT_ARGS, 0, 1,        ResourceManager::CMD_HELP_INFO_DB             }, // COMMAND_INFO_DB
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT | FLAG_UI,         1, 1,        ResourceManager::CMD_HELP_SEARCH              }, // COMMAND_SEARCH
	{ CTX_SYSTEM,                                       0, 0,        ResourceManager::CMD_HELP_SHOW_IGNORE_LIST    }, // COMMAND_SHOW_IGNORE_LIST
	{ CTX_SYSTEM,                                       0, 0,        ResourceManager::CMD_HELP_SHOW_EXTRA_SLOTS    }, // COMMAND_SHOW_EXTRA_SLOTS
	{ CTX_GENERAL_CHAT | FLAG_UI,                       0, 0,        ResourceManager::CMD_HELP_WINAMP              }, // COMMAND_MEDIA_PLAYER
	{ CTX_SYSTEM | FLAG_UI,                             1, 1,        ResourceManager::CMD_HELP_WEB_SEARCH          }, // COMMAND_WEB_SEARCH
	{ CTX_SYSTEM | FLAG_UI | FLAG_SPLIT_ARGS,           1, 1,        ResourceManager::CMD_HELP_OPEN_URL            }, // COMMAND_OPEN_URL
	{ CTX_SYSTEM | FLAG_UI | FLAG_SPLIT_ARGS,           0, 1,        ResourceManager::CMD_HELP_OPEN_LOG            }, // COMMAND_OPEN_LOG
	{ CTX_SYSTEM | FLAG_UI | FLAG_SPLIT_ARGS,           0, 0,        ResourceManager::CMD_HELP_SHUTDOWN            }, // COMMAND_SHUTDOWN
	{ CTX_SYSTEM | FLAG_UI | FLAG_SPLIT_ARGS,           1, 1,        ResourceManager::CMD_HELP_WHOIS               }, // COMMAND_WHOIS
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        ResourceManager::CMD_HELP_GEOIP               }, // COMMAND_GEOIP
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        ResourceManager::CMD_HELP_PG_INFO             }, // COMMAND_PG_INFO
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     2, UINT_MAX, 0                                             }, // COMMAND_USER
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        0                                             }, // COMMAND_USER_CONNECTIONS
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        0                                             }, // COMMAND_USER_IP
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 2,        0                                             }, // COMMAND_QUEUE
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, UINT_MAX, 0                                             }, // COMMAND_DHT
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, UINT_MAX, 0                                             }, // COMMAND_TTH
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, UINT_MAX, 0                                             }, // COMMAND_IP_BANS
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        0                                             }, // COMMAND_DEBUG_ADD_TREE
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 1,        0                                             }, // COMMAND_DEBUG_DISABLE
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, UINT_MAX, 0                                             }, // COMMAND_DEBUG_BLOOM
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     1, 2,        0                                             }, // COMMAND_DEBUG_MYSHARE
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     0, UINT_MAX, 0                                             }, // COMMAND_DEBUG_GDI_INFO
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     2, UINT_MAX, 0                                             }, // COMMAND_DEBUG_HTTP
	{ CTX_SYSTEM,                                       0, 0,        0                                             }, // COMMAND_DEBUG_UNKNOWN_TAGS
	{ CTX_SYSTEM | FLAG_SPLIT_ARGS,                     2, 2,        0                                             }, // COMMAND_DEBUG_DIVIDE
	{ CTX_GENERAL_CHAT,                                 1, 1,        ResourceManager::CMD_HELP_SAY                 }, // COMMAND_SAY
	{ CTX_GENERAL_CHAT,                                 1, 1,        ResourceManager::CMD_HELP_ME                  }, // COMMAND_ME
	{ CTX_HUB,                                          1, 1,        ResourceManager::CMD_HELP_LAST_NICK           }, // COMMAND_LAST_NICK
	{ CTX_SYSTEM | FLAG_GENERAL_CHAT,                   0, UINT_MAX, ResourceManager::CMD_HELP_HELP                }  // COMMAND_HELP
};

struct CommandName
{
	string name;
	int command;
	bool canonical;
};

// This list is sorted
static const CommandName names[] =
{
	{ "addtree",        COMMAND_DEBUG_ADD_TREE      },
	{ "away",           COMMAND_AWAY                },
	{ "bloom",          COMMAND_DEBUG_BLOOM         },
	{ "c",              COMMAND_CLEAR               },
	{ "ccpm",           COMMAND_CCPM                },
	{ "clear",          COMMAND_CLEAR               },
	{ "close",          COMMAND_CLOSE               },
	{ "cls",            COMMAND_CLEAR               },
	{ "con",            COMMAND_INFO_CONNECTION     },
	{ "connection",     COMMAND_INFO_CONNECTION     },
	{ "cpu",            COMMAND_INFO_CPU            },
	{ "dbinfo",         COMMAND_INFO_DB             },
	{ "dht",            COMMAND_DHT                 },
	{ "di",             COMMAND_INFO_STORAGE        },
	{ "disable",        COMMAND_DEBUG_DISABLE       },
	{ "disks",          COMMAND_INFO_STORAGE        },
	{ "divide",         COMMAND_DEBUG_DIVIDE        },
	{ "dsp",            COMMAND_INFO_DISK_SPACE     },
	{ "extraslots",     COMMAND_SET_EXTRA_SLOTS     },
	{ "fav",            COMMAND_ADD_FAVORITE        },
	{ "favorite",       COMMAND_ADD_FAVORITE        },
	{ "favshowjoins",   COMMAND_FAV_SHOW_JOINS      },
	{ "find",           COMMAND_FIND_TEXT           },
	{ "flushdb",        COMMAND_FLUSH_STATS         },
	{ "foobar",         COMMAND_MEDIA_PLAYER        },
	{ "g",              COMMAND_WEB_SEARCH          },
	{ "gdiinfo",        COMMAND_DEBUG_GDI_INFO      },
	{ "geoip",          COMMAND_GEOIP               },
	{ "getlist",        COMMAND_GET_LIST            },
	{ "gl",             COMMAND_GET_LIST            },
	{ "google",         COMMAND_WEB_SEARCH          },
	{ "grant",          COMMAND_GRANT_EXTRA_SLOT    },
	{ "grants",         COMMAND_SHOW_EXTRA_SLOTS    },
	{ "h",              COMMAND_HELP                },
	{ "help",           COMMAND_HELP                },
	{ "http",           COMMAND_DEBUG_HTTP          },
	{ "ignorelist",     COMMAND_SHOW_IGNORE_LIST    },
	{ "il",             COMMAND_SHOW_IGNORE_LIST    },
	{ "ipbans",         COMMAND_IP_BANS             },
	{ "ipupdate",       COMMAND_IP_UPDATE           },
	{ "itunes",         COMMAND_MEDIA_PLAYER        },
	{ "ja",             COMMAND_MEDIA_PLAYER        },
	{ "join",           COMMAND_JOIN                },
	{ "limit",          COMMAND_LIMIT               },
	{ "log",            COMMAND_OPEN_LOG            },
	{ "makefilelist",   COMMAND_MAKE_FILE_LIST      },
	{ "mcpm",           COMMAND_MC_PRIVATE_MESSAGE  },
	{ "me",             COMMAND_ME                  },
	{ "mpc",            COMMAND_MEDIA_PLAYER        },
	{ "myshare",        COMMAND_DEBUG_MYSHARE       },
	{ "n",              COMMAND_LAST_NICK           },
	{ "nick",           COMMAND_LAST_NICK           },
	{ "password",       COMMAND_PASSWORD            },
	{ "pginfo",         COMMAND_PG_INFO             },
	{ "pm",             COMMAND_PRIVATE_MESSAGE     },
	{ "qcd",            COMMAND_MEDIA_PLAYER        },
	{ "queue",          COMMAND_QUEUE               },
	{ "ratio",          COMMAND_INFO_RATIO          },
	{ "refresh",        COMMAND_REFRESH_SHARE       },
	{ "remfav",         COMMAND_REMOVE_FAVORITE     },
	{ "removefav",      COMMAND_REMOVE_FAVORITE     },
	{ "removefavorite", COMMAND_REMOVE_FAVORITE     },
	{ "s",              COMMAND_SEARCH              },
	{ "savequeue",      COMMAND_SAVE_QUEUE          },
	{ "say",            COMMAND_SAY                 },
	{ "search",         COMMAND_SEARCH              },
	{ "sharefile",      COMMAND_SHARE_FILE          },
	{ "showjoins",      COMMAND_SHOW_JOINS          },
	{ "shutdown",       COMMAND_SHUTDOWN            },
	{ "sl",             COMMAND_SET_SLOTS           },
	{ "slots",          COMMAND_SET_SLOTS           },
	{ "smallfilesize",  COMMAND_SET_SMALL_FILE_SIZE },
	{ "speed",          COMMAND_INFO_SPEED          },
	{ "sq",             COMMAND_SAVE_QUEUE          },
	{ "stats",          COMMAND_INFO_STATS          },
	{ "switch",         COMMAND_USER_LIST_LOCATION  },
	{ "sysinfo",        COMMAND_INFO_SYSTEM         },
	{ "systeminfo",     COMMAND_INFO_SYSTEM         },
	{ "ts",             COMMAND_TIMESTAMPS          },
	{ "tth",            COMMAND_TTH                 },
	{ "u",              COMMAND_OPEN_URL            },
	{ "uconn",          COMMAND_USER_CONNECTIONS    },
	{ "ungrant",        COMMAND_UNGRANT_EXTRA_SLOT  },
	{ "unknowntags",    COMMAND_DEBUG_UNKNOWN_TAGS  },
	{ "uptime",         COMMAND_INFO_UPTIME         },
	{ "user",           COMMAND_USER                },
	{ "userip",         COMMAND_USER_IP             },
	{ "userlist",       COMMAND_TOGGLE_USER_LIST    },
	{ "ut",             COMMAND_INFO_UPTIME         },
	{ "ver",            COMMAND_INFO_VERSION        },
	{ "version",        COMMAND_INFO_VERSION        },
	{ "w",              COMMAND_MEDIA_PLAYER        },
	{ "whois",          COMMAND_WHOIS               },
	{ "winamp",         COMMAND_MEDIA_PLAYER, true  },
	{ "wmp",            COMMAND_MEDIA_PLAYER        }
};

static inline bool isWhiteSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void trimRight(string& s)
{
	string::size_type len = s.length();
	while (len && isWhiteSpace(s[len-1])) --len;
	s.resize(len);
}

static void trimLeft(string& s)
{
	string::size_type i = 0;
	while (i < s.length() && isWhiteSpace(s[i])) ++i;
	if (i) s.erase(0, i);
}

static inline void trim(string& s)
{
	trimRight(s);
	trimLeft(s);
}

bool Commands::parseCommand(const string& str, ParsedCommand& pc)
{
	pc.frameId = 0;
	if (str.empty() || str[0] != '/') return false;
	string::size_type i = str.find(' ');
	string cmd, param;
	if (i != string::npos)
	{
		param = str.substr(i + 1);
		cmd = str.substr(1, i - 1);
		trim(param);
	}
	else
		cmd = str.substr(1);
	trimRight(cmd);
	Text::makeLower(cmd);

	static const size_t nameCount = sizeof(names)/sizeof(names[0]);
	auto j = std::lower_bound(names, names + nameCount, cmd, [](const CommandName& x, const string& y) { return x.name < y; });
	if (j == names + nameCount || j->name != cmd)
	{
		pc.args = { cmd };
		return false;
	}
	pc.command = j->command;

	const CommandDescription& cd = desc[pc.command];
	pc.args = { cmd };
	if (!param.empty())
	{
		if (cd.flags & FLAG_GET_FIRST_ARG)
		{
			i = param.find(' ');
			if (i != string::npos)
			{
				pc.args.push_back(param.substr(0, i));
				string tmp = param.substr(i + 1);
				trim(tmp);
				if (!tmp.empty()) pc.args.emplace_back(std::move(tmp));
			}
		}
		else if (cd.flags & FLAG_SPLIT_ARGS)
		{
			for (string::size_type i = 0; i < param.length(); ++i)
			{
				if (isWhiteSpace(param[i])) continue;
				string newParam;
				bool quote = false;
				for (; i < param.length() && (quote || !isWhiteSpace(param[i])); ++i)
				{
					if (param[i] == '"')
					{
						quote = !quote;
						continue;
					}
					newParam += param[i];
				}
				if (!newParam.empty())
					pc.args.emplace_back(std::move(newParam));
			}
		}
		else
			pc.args.emplace_back(std::move(param));
	}
	return true;
}

bool Commands::checkArguments(const ParsedCommand& pc, string& errorText)
{
	const CommandDescription& cd = desc[pc.command];
	size_t args = pc.args.size() - 1;
	if (args < cd.minArgs)
	{
		if (cd.minArgs > 1)
			errorText = STRING_F(COMMAND_N_ARGS_REQUIRED, cd.minArgs);
		else
			errorText = STRING(COMMAND_ARG_REQUIRED);
		return false;
	}
	if (args > cd.maxArgs)
	{
		if (cd.maxArgs)
			errorText = STRING_F(COMMAND_N_ARGS_EXTRA, cd.maxArgs);
		else
			errorText = STRING(COMMAND_NO_ARGS_REQUIRED);
		return false;
	}
	return true;
}

static int getAction(const ParsedCommand& pc, const char* actions[])
{
	if (pc.args.size() < 2) return 0;
	const string& s = pc.args[1];
	for (int i = 0; actions[i]; ++i)
		if (stricmp(s.c_str(), actions[i]) == 0)
			return i + 1;
	return 0;
}

static const char* actionsTTH[] = { "info", "addtree", "rmtree", nullptr };
enum
{
	ACTION_TTH_INFO = 1,
	ACTION_TTH_ADD_TREE,
	ACTION_TTH_REMOVE_TREE
};

static const char* actionsUserConnections[] = { "list", "expect", "tokens", "suppress", nullptr };
enum
{
	ACTION_UCONN_LIST = 1,
	ACTION_UCONN_EXPECT,
	ACTION_UCONN_TOKENS,
	ACTION_UCONN_SUPPRESS
};

static const char* actionsDHT[] = { "info", "nodes", "find", "fnode", "ping", "publish", nullptr };
enum
{
	ACTION_DHT_INFO = 1,
	ACTION_DHT_NODES,
	ACTION_DHT_FIND,
	ACTION_DHT_FIND_NODE,
	ACTION_DHT_PING,
	ACTION_DHT_PUBLISH
};

static const char* actionsUser[] = { "info", "getlist", "mq", "dldir", "stat", "rmstat", nullptr };
enum
{
	ACTION_USER_INFO = 1,
	ACTION_USER_GET_LIST,
	ACTION_USER_MATCH_QUEUE,
	ACTION_USER_DL_DIR,
	ACTION_USER_STAT,
	ACTION_USER_REMOVE_STAT
};

static const char* actionsQueue[] = { "info", "remove", nullptr };
enum
{
	ACTION_QUEUE_INFO = 1,
	ACTION_QUEUE_REMOVE
};

static const char* actionsDisable[] = { "partial", nullptr };
enum
{
	ACTION_DISABLE_PARTIAL = 1
};

static const char* actionsBloom[] = { "info", "match", nullptr };
enum
{
	ACTION_BLOOM_INFO = 1,
	ACTION_BLOOM_MATCH
};

static const char* actionsMyShare[] = { "info", "search", nullptr };
enum
{
	ACTION_MYSHARE_INFO = 1,
	ACTION_MYSHARE_SEARCH
};

static const char* actionsHttp[] = { "get", "post", nullptr };
enum
{
	ACTION_HTTP_GET = 1,
	ACTION_HTTP_POST
};

static const char* actionsIP[] = { "v4", "v6", nullptr };
enum
{
	ACTION_IP_V4 = 1,
	ACTION_IP_V6
};

static const char* actionsIpBans[] = { "info", "remove", "protect", "unprotect", nullptr };
enum
{
	ACTION_IPBANS_INFO = 1,
	ACTION_IPBANS_REMOVE,
	ACTION_IPBANS_PROTECT,
	ACTION_IPBANS_UNPROTECT
};

bool Commands::isPublic(const StringList& args)
{
	static string pub("pub");
	return args.size() > 1 && Text::asciiEqual(args[1], pub);
}

static bool parseCID(CID& cid, const string& param, Commands::Result* res)
{
	bool error;
	if (param.length() == 39)
		Util::fromBase32(param.c_str(), cid.writableData(), CID::SIZE, &error);
	else
		error = true;
	if (error)
	{
		if (res)
		{
			res->text = STRING(COMMAND_INVALID_CID);
			res->what = RESULT_ERROR_MESSAGE;
		}
		return false;
	}
	return true;
}

static bool parseTTH(TTHValue& tth, const string& param, Commands::Result& res)
{
	bool error;
	if (param.length() == 39)
		Util::fromBase32(param.c_str(), tth.data, TTHValue::BYTES, &error);
	else
		error = true;
	if (error)
	{
		res.text = STRING(INVALID_TTH);
		res.what = RESULT_ERROR_MESSAGE;
		return false;
	}
	return true;
}

static uint64_t getBlockSize(uint64_t fileSize, unsigned leafCount)
{
	if (!leafCount) return 0;
	uint64_t bl = 1024;
	while (bl * leafCount < fileSize) bl <<= 1;
	return bl;
}

bool Commands::processCommand(const ParsedCommand& pc, Result& res)
{
	if (!checkArguments(pc, res.text))
	{
		res.what = RESULT_ERROR_MESSAGE;
		return true;
	}
	switch (pc.command)
	{
		case COMMAND_HELP:
			res.text = STRING(CMD_AVAILABLE_COMMANDS) + '\n' + getHelpText();
			res.what = RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_REFRESH_SHARE:
			try
			{
				ShareManager::getInstance()->refreshShare();
				res.text = STRING(REFRESHING_SHARE);
				res.what = RESULT_LOCAL_TEXT;
			}
			catch (const ShareException& e)
			{
				res.text = e.getError();
				res.what = RESULT_ERROR_MESSAGE;
			}
			return true;

		case COMMAND_MAKE_FILE_LIST:
			ShareManager::getInstance()->generateFileList();
			res.text = STRING(COMMAND_DONE);
			res.what = RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_SHARE_FILE:
		{
			const string& path = pc.args[1];
			string dir = Util::getFilePath(path);
			if (!ShareManager::getInstance()->isDirectoryShared(dir))
			{
				res.text = STRING(DIRECTORY_NOT_SHARED);
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			TigerTree tree;
			std::atomic_bool stopFlag(false);
			if (!Util::getTTH(path, true, 512 * 1024, stopFlag, tree))
			{
				res.text = STRING(COMMAND_TTH_ERROR);
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			try
			{
				ShareManager::getInstance()->addFile(path, tree.getRoot());
				auto db = DatabaseManager::getInstance();
				auto hashDb = db->getHashDatabaseConnection();
				if (hashDb)
				{
					db->addTree(hashDb, tree);
					db->putHashDatabaseConnection(hashDb);
				}
				res.text = STRING_F(COMMAND_FILE_SHARED, Util::getMagnet(tree.getRoot(), Util::getFileName(path), tree.getFileSize()));
				res.what = RESULT_LOCAL_TEXT;
			}
			catch (Exception& e)
			{
				res.text = e.getError();
				res.what = RESULT_ERROR_MESSAGE;
			}
			return true;
		}
#ifdef _DEBUG
		case COMMAND_DEBUG_ADD_TREE:
		{
			TigerTree tree;
			std::atomic_bool stopFlag(false);
			if (!Util::getTTH(pc.args[1], true, 512 * 1024, stopFlag, tree))
			{
				res.text = STRING(COMMAND_TTH_ERROR);
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			auto db = DatabaseManager::getInstance();
			auto hashDb = db->getHashDatabaseConnection();
			if (hashDb && db->addTree(hashDb, tree))
			{
				db->putHashDatabaseConnection(hashDb);
				res.text = STRING_F(COMMAND_TTH_ADDED, tree.getRoot().toBase32());
				res.what = RESULT_LOCAL_TEXT;
			}
			else
			{
				if (hashDb) db->putHashDatabaseConnection(hashDb);
				res.text = "Unable to add tree";
				res.what = RESULT_ERROR_MESSAGE;
			}
			return true;
		}
#endif
		case COMMAND_SAVE_QUEUE:
			QueueManager::getInstance()->saveQueue();
			res.text = STRING(QUEUE_SAVED);
			res.what = RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_SHOW_IGNORE_LIST:
		{
			string text = UserManager::getInstance()->getIgnoreListAsString();
			if (text.empty())
				res.text = STRING(COMMAND_EMPTY_LIST);
			else
				res.text = STRING(IGNORED_USERS) + ':' + text;
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
		case COMMAND_SHOW_EXTRA_SLOTS:
		{
			vector<UploadManager::ReservedSlotInfo> info;
			UploadManager::getInstance()->getReservedSlots(info);
			uint64_t currentTick = GET_TICK();
			for (const auto& rs : info)
			{
				uint64_t seconds = rs.timeout < currentTick ? 0 : (rs.timeout-currentTick)/1000;
				if (!res.text.empty()) res.text += '\n';
				res.text += rs.user->getLastNick() + '/' + rs.user->getCID().toBase32();
				res.text += " timeout: " + Util::toString(seconds);
			}
			if (res.text.empty()) res.text = STRING(COMMAND_EMPTY_LIST);
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
		case COMMAND_SET_SLOTS:
		{
			int n = Util::toInt(pc.args[1]);
			if (n > 0)
			{
				auto ss = SettingsManager::instance.getCoreSettings();
				ss->lockWrite();
				ss->setInt(Conf::SLOTS, n);
				ss->unlockWrite();
				ClientManager::infoUpdated();
				res.text = STRING(SLOTS_SET);
				res.what = RESULT_LOCAL_TEXT;
			}
			else
			{
				res.text = STRING(INVALID_NUMBER_OF_SLOTS);
				res.what = RESULT_ERROR_MESSAGE;
			}
			return true;
		}
		case COMMAND_SET_EXTRA_SLOTS:
		{
			int n = Util::toInt(pc.args[1]);
			if (n > 0)
			{
				auto ss = SettingsManager::instance.getCoreSettings();
				ss->lockWrite();
				ss->setInt(Conf::EXTRA_SLOTS, n);
				ss->unlockWrite();
				res.text = STRING(EXTRA_SLOTS_SET);
				res.what = RESULT_LOCAL_TEXT;
			}
			else
			{
				res.text = STRING(INVALID_NUMBER_OF_SLOTS);
				res.what = RESULT_ERROR_MESSAGE;
			}
			return true;
		}
		case COMMAND_SET_SMALL_FILE_SIZE:
		{
			int n = Util::toInt(pc.args[1]);
			if (n >= 64)
			{
				auto ss = SettingsManager::instance.getCoreSettings();
				ss->lockWrite();
				ss->setInt(Conf::MINISLOT_SIZE, n);
				ss->unlockWrite();
				res.text = STRING(SMALL_FILE_SIZE_SET);
				res.what = RESULT_LOCAL_TEXT;
			}
			else
			{
				res.text = STRING(INVALID_SIZE);
				res.what = RESULT_ERROR_MESSAGE;
			}
			return true;
		}
		case COMMAND_INFO_VERSION:
			res.text = APPNAME " " VERSION_STR;
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;
#ifdef _WIN32
		case COMMAND_INFO_UPTIME:
			res.text = "+me Uptime: " + Util::formatTime(Util::getUpTime()) + ". System uptime: " + Util::formatTime(SysInfo::getSysUptime());
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_INFO_SYSTEM:
			res.text = "+me systeminfo: " + AppStats::getFullSystemStatusMessage();
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_INFO_SPEED:
			res.text = "My Speed:\n" + AppStats::getSpeedInfo();
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_INFO_CPU:
			res.text  = "My CPU: " + SysInfo::getCPUInfo();
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_INFO_DISK_SPACE:
			res.text ="My Disk Space:\n" + AppStats::getDiskSpaceInfo();
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_INFO_STORAGE:
			res.text = "My Disks:\n" + AppStats::getDiskInfo();
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;

		case COMMAND_INFO_STATS:
			res.text = AppStats::getStats();
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;
#endif
#ifdef BL_FEATURE_IP_DATABASE
		case COMMAND_INFO_RATIO:
		{
			StringMap params;
			auto dm = DatabaseManager::getInstance();
			dm->loadGlobalRatio();
			const DatabaseManager::GlobalRatio& ratio = dm->getGlobalRatio();
			double r = ratio.download > 0 ? (double) ratio.upload / (double) ratio.download : 0;
			params["ratio"] = Util::toString(r);
			params["up"] = Util::formatBytes(ratio.upload);
			params["down"] = Util::formatBytes(ratio.download);
			auto ss = SettingsManager::instance.getCoreSettings();
			ss->lockRead();
			string message = ss->getString(Conf::RATIO_MESSAGE);
			ss->unlockRead();
			res.text = Util::formatParams(message, params, false);
			res.what = isPublic(pc.args) ? RESULT_TEXT : RESULT_LOCAL_TEXT;
			return true;
		}
#endif // BL_FEATURE_IP_DATABASE
		case COMMAND_GEOIP:
		{
			IpAddress addr;
			if (!Util::parseIpAddress(addr, pc.args[1]))
			{
				res.text = STRING(COMMAND_INVALID_IP);
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			IPInfo ipInfo;
			Util::getIpInfo(addr, ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
			if (!ipInfo.country.empty() || !ipInfo.location.empty())
			{
				res.text = STRING(LOCATION_BARE) + ": ";
				if (!ipInfo.country.empty() && !ipInfo.location.empty())
				{
					res.text += ipInfo.country;
					res.text += ", ";
				}
				res.text += Util::getDescription(ipInfo);
			}
			else
				res.text = "Location not found";
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
		case COMMAND_PG_INFO:
		{
			uint32_t addr;
			if (!Util::parseIpAddress(addr, pc.args[1]))
			{
				res.text = STRING(COMMAND_INVALID_IP);
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			IPInfo ipInfo;
			IpAddress ip;
			ip.type = AF_INET;
			ip.data.v4 = addr;
			Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_P2P_GUARD);
			if (!ipInfo.p2pGuard.empty())
				res.text += Util::printIpAddress(addr) + ": " + ipInfo.p2pGuard;
			else
				res.text = "IP not found";
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
		case COMMAND_TTH:
		{
			int action = getAction(pc, actionsTTH);
			if (action == ACTION_TTH_INFO)
			{
				if (pc.args.size() < 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				TTHValue tth;
				if (!parseTTH(tth, pc.args[2], res)) return true;
				string path;
				uint64_t fileSize;
				size_t treeSize;
				uint32_t uploadCount;
				unsigned flags;
				string tthText = "TTH " + pc.args[2] + ": ";
				res.text = tthText;
				auto db = DatabaseManager::getInstance();
				auto hashDb = db->getHashDatabaseConnection();
				if (!(hashDb && hashDb->getFileInfo(tth.data, flags, &fileSize, &path, &treeSize, &uploadCount)))
				{
					res.text += "not found in database\n";
				}
				else
				{
					res.text += "found in database, flags=" + Util::toString(flags);
					res.text += ", fileSize=" + Util::toString(fileSize);
					if (!path.empty()) { res.text += ", path="; res.text += path; }
					if (treeSize)
					{
						res.text += ", treeSize=";
						res.text += Util::toString(treeSize);
						res.text += ", blockSize=";
						res.text += Util::toString(getBlockSize(fileSize, treeSize / TigerTree::BYTES));
					}
					if (uploadCount) { res.text += ", uploadCount="; res.text += Util::toString(uploadCount); }
					res.text += '\n';
				}
				if (hashDb) db->putHashDatabaseConnection(hashDb);
				res.text += tthText;
				int64_t size;
				if (!ShareManager::getInstance()->getFileInfo(tth, path, size))
					res.text += "not found in share\n";
				else
					res.text += "found in share, path=" + path;
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_TTH_ADD_TREE)
			{
				if (pc.args.size() < 4)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 3);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				TigerTree tree;
				try
				{
					int64_t fileSize = Util::toInt64(pc.args[3]);
					File f(pc.args[2], File::READ, File::OPEN, false);
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
					auto db = DatabaseManager::getInstance();
					auto hashDb = db->getHashDatabaseConnection();
					if (hashDb && db->addTree(hashDb, tree))
					{
						db->putHashDatabaseConnection(hashDb);
						res.text = STRING_F(COMMAND_TTH_ADDED, tree.getRoot().toBase32());
						res.what = RESULT_LOCAL_TEXT;
					}
					else
					{
						if (hashDb) db->putHashDatabaseConnection(hashDb);
						res.text = "Unable to add tree";
						res.what = RESULT_ERROR_MESSAGE;
					}
				}
				catch (Exception& e)
				{
					res.text = e.getError();
					res.what = RESULT_ERROR_MESSAGE;
				}
				return true;
			}
			if (action == ACTION_TTH_REMOVE_TREE)
			{
				if (pc.args.size() < 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				TTHValue tth;
				if (!parseTTH(tth, pc.args[2], res)) return true;
				auto db = DatabaseManager::getInstance();
				auto hashDb = db->getHashDatabaseConnection();
				if (hashDb && hashDb->removeTigerTree(tth.data))
				{
					db->putHashDatabaseConnection(hashDb);
					res.text = STRING(DONE);
					res.what = RESULT_LOCAL_TEXT;
				}
				else
				{
					if (hashDb) db->putHashDatabaseConnection(hashDb);
					res.text = "Unable to remove tree";
					res.what = RESULT_ERROR_MESSAGE;
				}
				return true;
			}
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
		case COMMAND_USER_CONNECTIONS:
		{
			int action = getAction(pc, actionsUserConnections);
			if (action == ACTION_UCONN_LIST)
			{
				res.text = ConnectionManager::getInstance()->getUserConnectionInfo();
				if (res.text.empty()) res.text = STRING(COMMAND_EMPTY_LIST);
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_UCONN_EXPECT)
			{
				res.text = ConnectionManager::getInstance()->getExpectedInfo();
				if (res.text.empty()) res.text = STRING(COMMAND_EMPTY_LIST);
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_UCONN_TOKENS)
			{
				res.text = ConnectionManager::getInstance()->getTokenInfo();
				if (res.text.empty()) res.text = STRING(COMMAND_EMPTY_LIST);
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
#ifdef _DEBUG
			if (action == ACTION_UCONN_SUPPRESS)
			{
				suppressUserConn = !suppressUserConn;
				res.text = "Suppress: " + Util::toString(suppressUserConn);
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
#endif
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
		case COMMAND_DHT:
		{
			int action = getAction(pc, actionsDHT);
			if (action == ACTION_DHT_INFO)
			{
				dht::DHT* d = dht::DHT::getInstance();
				res.text = "DHT port: " + Util::toString(d->getPort()) + '\n';
				string externalIp;
				bool isFirewalled;
				d->getPublicIPInfo(externalIp, isFirewalled);
				res.text += "External IP: " + externalIp;
				res.text += isFirewalled ? " (firewalled)" : " (open)";
				res.text += "\nConnected: ";
				res.text += d->isConnected() ? "yes" : "no";
				res.text += "\nState: ";
				res.text += Util::toString(d->getState());
				res.text += '\n';
				size_t nodeCount = 0;
				{
					dht::DHT::LockInstanceNodes lock(d);
					const auto nodes = lock.getNodes();
					if (nodes) nodeCount = nodes->size();
				}
				res.text += "Nodes: " + Util::toString(nodeCount) + '\n';
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_DHT_NODES)
			{
				unsigned maxType = 4;
				if (pc.args.size() >= 3) maxType = Util::toInt(pc.args[2]);
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
				res.text  = "Nodes: " + Util::toString(nv.size()) + '\n';
				for (const auto& node : nv)
				{
					const UserPtr& user = node->getUser();
					res.text += user->getCID().toBase32();
					res.text += ": ";
					const Identity& id = node->getIdentity();
					string nick = id.getNick();
					res.text += nick.empty() ? "<empty>" : nick;
					res.text += ' ';
					res.text += Util::printIpAddress(id.getIP4());
					res.text += ':';
					res.text += Util::toString(id.getUdp4Port());
					uint64_t expires = node->getExpires();
					if (expires)
					{
						int seconds = 0;
						if (expires > now) seconds = static_cast<int>((expires - now) / 1000);
						res.text += " Expires=" + Util::toString(seconds);
					}
					res.text += " Type=" + Util::toString(node->getType());
					if (node->isIpVerified()) res.text += " Verified";
					res.text += '\n';
				}
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_DHT_FIND)
			{
				if (pc.args.size() < 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				TTHValue tth;
				if (!parseTTH(tth, pc.args[2], res)) return true;
				dht::DHT::getInstance()->findFile(tth.toBase32(), Util::rand(), 0);
				res.text = "DHT: file search started";
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_DHT_FIND_NODE)
			{
				if (pc.args.size() < 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				CID cid;
				if (!parseCID(cid, pc.args[2], &res)) return true;
				dht::SearchManager::getInstance()->findNode(cid);
				res.text = "DHT: node search started";
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_DHT_PING)
			{
				if (pc.args.size() < 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				CID cid;
				if (!parseCID(cid, pc.args[2], &res)) return true;
				if (dht::DHT::getInstance()->pingNode(cid))
				{
					res.text = "DHT: pinging node";
					res.what = RESULT_LOCAL_TEXT;
				}
				else
				{
					res.text = "Node not found";
					res.what = RESULT_ERROR_MESSAGE;
				}
				return true;
			}
			if (action == ACTION_DHT_PUBLISH)
			{
				if (pc.args.size() < 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				TTHValue tth;
				if (!parseTTH(tth, pc.args[2], res)) return true;
				string filename;
				int64_t size;
				if (!ShareManager::getInstance()->getFileInfo(tth, filename, size))
				{
					res.text = "File not found";
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				auto im = dht::IndexManager::getInstance();
				if (im)
				{
					im->publishFile(tth, size);
					res.text = "Publishing file " + filename + " (";
					res.text += Util::toString(size);
					res.text += ')';
					res.what = RESULT_LOCAL_TEXT;
				}
				else
				{
					res.text = "Could not publish this file";
					res.what = RESULT_ERROR_MESSAGE;
				}
				return true;
			}
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
		case COMMAND_USER:
		{
			int action = getAction(pc, actionsUser);
			if (!action)
			{
				res.text = STRING(COMMAND_INVALID_ACTION);
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			bool hasCID = false;
			string hubUrl;
			CID cid;
			if (parseCID(cid, pc.args[2], nullptr)) hasCID = true;
			if (pc.args.size() >= 4 && !hasCID)
			{
				hubUrl = pc.args[3];
			}
			else if (!hasCID)
			{
				res.text = "Hub URL must be specified";
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			if (!hasCID) cid = ClientManager::makeCid(pc.args[2], hubUrl);
			if (action == ACTION_USER_STAT || action == ACTION_USER_REMOVE_STAT)
			{
#ifdef BL_FEATURE_IP_DATABASE
				UserStatItem userStat;
				std::unique_ptr<IPStatMap> ipStatPtr;
				auto db = DatabaseManager::getInstance();
				auto conn = db->getConnection();
				if (conn)
				{
					if (action == ACTION_USER_REMOVE_STAT)
					{
						conn->removeIPStat(cid);
						conn->removeUserStat(cid);
					}
					else
					{
						ipStatPtr.reset(conn->loadIPStat(cid));
						conn->loadUserStat(cid, userStat);
					}
					db->putConnection(conn);
				}
				res.what = RESULT_LOCAL_TEXT;
				if (action == ACTION_USER_REMOVE_STAT)
				{
					res.text = STRING(COMMAND_DONE);
					return true;
				}
				if (!ipStatPtr && !(userStat.flags & UserStatItem::FLAG_INSERTED))
				{
					res.text = "No information in DB";
					return true;
				}
				if (userStat.flags & UserStatItem::FLAG_INSERTED)
				{
					res.text += "Last IP: " + userStat.lastIp + '\n';
					res.text += "Messages: " + Util::toString(userStat.messageCount) + '\n';
					for (const string& s : userStat.nickList)
					{
						auto pos = s.find('\t');
						if (pos != string::npos)
							res.text += "Nick: " + s.substr(0, pos) + ", Hub: " + s.substr(pos + 1) + '\n';
					}
				}
				IPStatMap* ipStat = ipStatPtr.get();
				if (ipStat)
					for (auto i = ipStat->data.cbegin(); i != ipStat->data.cend(); ++i)
					{
						res.text += "IP " + i->first;
						res.text += ": downloaded=" + Util::toString(i->second.download);
						res.text += ", uploaded=" + Util::toString(i->second.upload);
						res.text += '\n';
					}
#else
				res.text = "Feature not available";
				res.what = RESULT_ERROR_MESSAGE;
#endif
				return true;
			}
			OnlineUserPtr ou = ClientManager::findOnlineUser(cid, hubUrl, !hubUrl.empty());
			if (!ou)
			{
				res.text = "User not found";
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			if (action == ACTION_USER_GET_LIST)
			{
				ou->getList();
				res.text = "Getting file list";
			}
			else if (action == ACTION_USER_MATCH_QUEUE || action == ACTION_USER_DL_DIR)
			{
				size_t index = hasCID ? 3 : 4;
				if (pc.args.size() <= index)
				{
					++index;
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, index);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				auto qm = QueueManager::getInstance();
				qm->addDirectory(pc.args[index], ou->getHintedUser(), Util::emptyString,
					QueueItem::DEFAULT,
					action == ACTION_USER_MATCH_QUEUE ? QueueManager::DIR_FLAG_MATCH_QUEUE : QueueManager::DIR_FLAG_DOWNLOAD_DIRECTORY);
				res.text = "Requesting partial file list";
			}
			else
			{
				ou->getIdentity().getReport(res.text);
			}
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
		case COMMAND_QUEUE:
		{
			int action = getAction(pc, actionsQueue);
			if (action == ACTION_QUEUE_INFO)
			{
				res.text = "Download queue\n";
				size_t fileCount;
				{
					QueueManager::LockFileQueueShared fileQueue;
					fileCount = fileQueue.getQueueL().size();
				}
				res.text += "Total files: " + Util::toString(fileCount);
				res.text += "\nRunning downloads: " + Util::toString(QueueManager::userQueue.getRunningCount());
				res.text += "\nDirectories: " + Util::toString(QueueManager::getInstance()->getDirectoryItemCount());
				res.text += '\n';
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			else if (action == ACTION_QUEUE_REMOVE)
			{
				if (pc.args.size() != 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				bool result = QueueManager::getInstance()->removeTarget(pc.args[2]);
				res.what = RESULT_LOCAL_TEXT;
				res.text = result ? STRING(COMMAND_DONE) : "No such item";
				return true;
			}
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
		case COMMAND_INFO_DB:
		{
			res.what = RESULT_LOCAL_TEXT;
			if (pc.args.size() == 2 && pc.args[1] == "lmdb-details")
			{
				auto db = DatabaseManager::getInstance();
				auto hashDb = db->getHashDatabaseConnection();
				HashDatabaseConnection::DbInfo info;
				if (hashDb && hashDb->getDBInfo(info, HashDatabaseConnection::GET_DB_INFO_DETAILS | HashDatabaseConnection::GET_DB_INFO_TREES | HashDatabaseConnection::GET_DB_INFO_DIGEST | HashDatabaseConnection::GET_DB_INFO_MAP_SIZE))
				{
					res.text = "LMDB information\n";
					res.text += "Items: " + Util::toString(info.numKeys);
					res.text += "\nPages: " + Util::toString(info.numPages);
					res.text += "\nMap size: " + Util::formatBytes(info.mapSize);
					res.text += "\nSize of all keys: " + Util::toString(info.totalKeysSize);
					res.text += "\nSize of all values: " + Util::toString(info.totalDataSize);
					res.text += "\nSize of all trees: " + Util::toString(info.totalTreesSize);
					res.text += "\nHash of all keys: " + Util::toBase32(info.keysHash, sizeof(info.keysHash));
					res.text += "\nHash of all values: " + Util::toBase32(info.dataHash, sizeof(info.dataHash));
				}
				else
				{
					res.text = "Unable to get LMDB info";
					res.what = RESULT_ERROR_MESSAGE;
				}
				if (hashDb) db->putHashDatabaseConnection(hashDb);
			}
			else if (pc.args.size() == 2 && pc.args[1] == "lmdb-conn")
			{
				const HashDatabaseLMDB& lmdb = DatabaseManager::getInstance()->getHashDatabase();
				res.text = "LMDB connections\n";
				string info = lmdb.getConnectionInfo();
				res.text += info.empty() ? STRING(COMMAND_EMPTY_LIST) : info;
			}
			else
				res.text = DatabaseManager::getInstance()->getDBInfo();
			return true;
		}
		case COMMAND_IP_UPDATE:
		{
			bool v4 = true;
			bool v6 = ConnectivityManager::hasIP6();
			if (pc.args.size() > 1)
			{
				int action = getAction(pc, actionsIP);
				if (!action)
				{
					res.text = STRING(COMMAND_INVALID_ARGUMENT);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				v4 = action == ACTION_IP_V4;
				v6 = !v4;
			}
			string msg;
			if (v4)
			{
				if (!g_ipTest.runTest(IpTest::REQ_IP4, pc.frameId, &msg))
					msg = STRING_F(PORT_TEST_ERROR_GETTING_IP, 4);
				res.text = std::move(msg);
			}
			if (v6)
			{
				if (!g_ipTest.runTest(IpTest::REQ_IP6, pc.frameId, &msg))
					msg = STRING_F(PORT_TEST_ERROR_GETTING_IP, 6);
				if (!res.text.empty()) res.text += '\n';
				res.text += msg;
			}
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
		case COMMAND_IP_BANS:
		{
			int action = getAction(pc, actionsIpBans);
			if (action == ACTION_IPBANS_INFO)
			{
				int64_t timestamp = GET_TICK();
				res.text = tcpBans.getInfo("TCP", timestamp) + udpBans.getInfo("UDP", timestamp);
				if (res.text.empty())
					res.text = STRING(COMMAND_EMPTY_LIST);
				else
					res.text.insert(0, "Banned addresses:\n");
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_IPBANS_REMOVE || action == ACTION_IPBANS_PROTECT || action == ACTION_IPBANS_UNPROTECT)
			{
				if (pc.args.size() < 4)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 3);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				IpBans* bans;
				if (!stricmp(pc.args[2], "tcp")) bans = &tcpBans;
				else if (!stricmp(pc.args[2], "udp")) bans = &udpBans;
				else
				{
					res.text = STRING(COMMAND_INVALID_ARGUMENT);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				uint16_t port = 0;
				string ipString;
				IpAddress ip;
				if (!Util::parseIpPort(pc.args[3], ipString, port) || !port || !Util::parseIpAddress(ip, ipString))
				{
					res.text = STRING(COMMAND_INVALID_IP);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				IpPortKey key;
				key.setIP(ip, port);
				if (action == ACTION_IPBANS_REMOVE)
					bans->removeBan(key);
				else
					bans->protect(key, action == ACTION_IPBANS_PROTECT);
				res.text = STRING(COMMAND_DONE);
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
#ifdef BL_FEATURE_IP_DATABASE
		case COMMAND_FLUSH_STATS:
		{
			ClientManager::flushRatio();
			res.text = STRING(COMMAND_DONE);
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
		case COMMAND_USER_IP:
		{
			IpAddress addr;
			const string& ipAddrText = pc.args[1];
			if (!Util::parseIpAddress(addr, ipAddrText))
			{
				res.text = STRING(COMMAND_INVALID_IP);
				res.what = RESULT_ERROR_MESSAGE;
				return true;
			}
			vector<DBCIDStatItem> items;
			auto db = DatabaseManager::getInstance();
			auto conn = db->getConnection();
			if (conn)
			{
				conn->loadIPStatByIP(ipAddrText, items);
				db->putConnection(conn);
			}
			for (const DBCIDStatItem& item : items)
			{
				res.text += item.cid.toBase32();
				res.text += ": downloaded=" + Util::toString(item.download);
				res.text += ", uploaded=" + Util::toString(item.upload);
				res.text += '\n';
			}
			if (res.text.empty())
				res.text = "IP not found";
			else
				res.text.insert(0, "IP statistics:\n");
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
#endif
#ifdef _DEBUG
		case COMMAND_DEBUG_DISABLE:
		{
			int action = getAction(pc, actionsDisable);
			if (action == ACTION_DISABLE_PARTIAL)
			{
				disablePartialListUploads = !disablePartialListUploads;
				res.text = "Partial list uploads disabled: " + Util::toString(disablePartialListUploads);
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
		case COMMAND_DEBUG_BLOOM:
		{
			int action = getAction(pc, actionsBloom);
			if (action == ACTION_BLOOM_INFO)
			{
				size_t size, used;
				ShareManager::getInstance()->getBloomInfo(size, used);
				res.text = "Size: " + Util::toString(size) + ", used: " + Util::toString(used)
					+ " (" + Util::toString((double) used * 100.0 / (double) size) + "%)";
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_BLOOM_MATCH)
			{
				if (pc.args.size() < 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				string textLower = Text::toLower(pc.args[2]);
				res.text = textLower;
				res.text += ShareManager::getInstance()->matchBloom(textLower) ? ": Match found" : ": No match";
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
		case COMMAND_DEBUG_HTTP:
		{
			int action = getAction(pc, actionsHttp);
			if (action == ACTION_HTTP_GET)
			{
				if (pc.args.size() < 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				HttpClient::Request req;
				auto ss = SettingsManager::instance.getCoreSettings();
				ss->lockRead();
				req.userAgent = ss->getString(Conf::HTTP_USER_AGENT);
				ss->unlockRead();
				req.outputPath = Util::getHttpDownloadsPath();
				File::ensureDirectory(req.outputPath);
				req.url = pc.args[2];
				req.maxRedirects = 5;
				req.frameId = pc.frameId;
				uint64_t id = httpClient.addRequest(req);
				if (id)
				{
					res.text = STRING_F(HTTP_REQ_STARTED, id);
					httpClient.startRequest(id);
				}
				else
					res.text = STRING(HTTP_REQ_INIT_FAILED);
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
#endif
#if defined(BL_FEATURE_COLLECT_UNKNOWN_FEATURES) || defined(BL_FEATURE_COLLECT_UNKNOWN_TAGS)
		case COMMAND_DEBUG_UNKNOWN_TAGS:
		{
			string s = AdcSupports::getCollectedUnknownTags();
			res.text = s.empty() ? STRING(COMMAND_EMPTY_LIST) : "Dumping collected tags\n" + s;
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
#endif
		case COMMAND_DEBUG_MYSHARE:
		{
			int action = getAction(pc, actionsMyShare);
			if (action == ACTION_MYSHARE_INFO)
			{
				auto sm = ShareManager::getInstance();
				res.text = "Total size: " + Util::formatExactSize(sm->getTotalSharedSize()) +
					", Total files: " + Util::toString(sm->getTotalSharedFiles()) +
					", TTH map size: " + Util::toString(sm->getSharedTTHCount());
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			if (action == ACTION_MYSHARE_SEARCH)
			{
				if (pc.args.size() != 3)
				{
					res.text = STRING_F(COMMAND_N_ARGS_REQUIRED, 2);
					res.what = RESULT_ERROR_MESSAGE;
					return true;
				}
				vector<SearchResultCore> results;
				NmdcSearchParam param;
				param.maxResults = 50;
				param.filter = pc.args[2];
				ShareManager::getInstance()->search(results, param, nullptr);
				if (!results.empty())
				{
					res.text = "Found " + Util::toString(results.size()) + " item(s):\n";
					for (const auto& item : results)
					{
						res.text += item.getFile();
						res.text += '\n';
					}
				}
				else
					res.text = "Nothing found";
				res.what = RESULT_LOCAL_TEXT;
				return true;
			}
			res.text = STRING(COMMAND_INVALID_ACTION);
			res.what = RESULT_ERROR_MESSAGE;
			return true;
		}
#ifdef TEST_CRASH_HANDLER
		case COMMAND_DEBUG_DIVIDE:
		{
			int a = Util::toInt(pc.args[1]);
			int b = Util::toInt(pc.args[2]);
			int result = a/b;
			res.text = "Your answer is " + Util::toString(result);
			res.what = RESULT_LOCAL_TEXT;
			return true;
		}
#endif
	}
	res.text = STRING(UNKNOWN_COMMAND) + " /" + pc.args[0];
	res.what = RESULT_ERROR_MESSAGE;
	return true;
}

static void getCommandNames(int cmd, StringList& res)
{
	res.clear();
	bool canonName = false;
	static const size_t nameCount = sizeof(names)/sizeof(names[0]);
	for (size_t i = 0; i < nameCount; ++i)
		if (names[i].command == cmd)
		{
			if (names[i].canonical)
			{
				canonName = true;
				res.insert(res.begin(), names[i].name);
			}
			else
				res.push_back(names[i].name);
		}
	if (!canonName)
		std::sort(res.begin(), res.end(),
			[](const auto& l, const auto& r) { return l.length() > r.length(); });
}

static void addHelp(string& s, unsigned flag, unsigned notFlag, unsigned formatFlags)
{
	StringList cmdNames;
	for (int i = 0; i < NR_COMMANDS; ++i)
	{
		const CommandDescription& d = desc[i];
		if ((d.flags & flag) && !(d.flags & notFlag) && d.infoText)
		{
			getCommandNames(i, cmdNames);
			dcassert(!cmdNames.empty());
			s += '/';
			s += cmdNames[0];
			string help = STRING_I((ResourceManager::Strings) d.infoText);
			if ((d.flags & (CTX_HUB | CTX_USER)) == (CTX_HUB | CTX_USER))
			{
				auto pos = help.find('\n');
				if (pos != string::npos)
				{
					if (flag == CTX_HUB)
						help.erase(pos);
					else
						help.erase(0, pos + 1);
				}
			}
			auto pos = help.find('\t');
			if (pos != string::npos)
			{
				s += ' ';
				s += help.substr(0, pos);
				help.erase(0, pos + 1);
			}
			if (cmdNames.size() > 1)
			{
				string aliases;
				for (size_t i = 1; i < cmdNames.size(); ++i)
				{
					if (i > 1) aliases += ", ";
					if (formatFlags & GHT_MARK_ALIASES) aliases += '*';
					aliases += '/';
					aliases += cmdNames[i];
					if (formatFlags & GHT_MARK_ALIASES) aliases += '*';
				}
				help += ' ';
				help += STRING_F(CMD_ALIASES, aliases);
			}
			s += '\t';
			s += help;
			s += '\n';
		}
	}
}

string Commands::getHelpText(unsigned flags)
{
	string s = STRING(CMD_TITLE_GENERAL_CHAT);
	s += '\n';
	addHelp(s, FLAG_GENERAL_CHAT, 0, flags);
	s += '\n';
	s += STRING(CMD_TITLE_HUB);
	s += '\n';
	addHelp(s, CTX_HUB, FLAG_GENERAL_CHAT, flags);
	s += '\n';
	s += STRING(CMD_TITLE_USER);
	s += '\n';
	addHelp(s, CTX_USER, FLAG_GENERAL_CHAT, flags);
	s += '\n';
	s += STRING(CMD_TITLE_SYSTEM);
	s += '\n';
	addHelp(s, CTX_SYSTEM, FLAG_GENERAL_CHAT, flags);
	return s;
}

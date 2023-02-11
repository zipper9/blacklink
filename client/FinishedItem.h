#ifndef FINISHED_ITEM_H_
#define FINISHED_ITEM_H_

#include "Text.h"
#include "HashValue.h"
#include "Util.h"

#ifdef NO_HINTED_USER
#else
#include "HintedUser.h"
#endif

class FinishedItem
{
		friend class FinishedManager;

	public:
		enum
		{
			COLUMN_FIRST,
			COLUMN_FILE = COLUMN_FIRST,
			COLUMN_TYPE,
			COLUMN_DONE,
			COLUMN_PATH,
			COLUMN_TTH,
			COLUMN_NICK,
			COLUMN_HUB,
			COLUMN_SIZE,
			COLUMN_SPEED,
			COLUMN_IP,
			COLUMN_NETWORK_TRAFFIC,
			COLUMN_LAST
		};

		FinishedItem(const string& target, const string& nick, const string& hubUrl, int64_t size, int64_t speed,
		             time_t time, const TTHValue& tth, const string& ip, int64_t actual, int64_t id) :
			target(target),
			hub(hubUrl),
			hubs(hubUrl),
			size(size),
			avgSpeed(speed),
			time(time),
			tth(tth),
			ip(ip),
			nick(nick),
			actual(actual),
			id(id),
			tempId(0)
		{
		}

#ifndef NO_HINTED_USER
		FinishedItem(const string& target, const HintedUser& user, const string& hubs, int64_t size, int64_t speed,
		             time_t time, const TTHValue& tth, const string& ip, int64_t actual) :
			target(target),
			cid(user.user->getCID()),
			hub(user.hint),
			hubs(hubs),
			size(size),
			avgSpeed(speed),
			time(time),
			tth(tth),
			ip(ip),
			nick(user.user->getLastNick()),
			actual(actual),
			id(0),
			tempId(0)
		{
		}
#endif

		tstring getText(int col) const;

		static int compareItems(const FinishedItem* a, const FinishedItem* b, int col)
		{
			switch (col)
			{
				case COLUMN_SPEED:
					return compare(a->getAvgSpeed(), b->getAvgSpeed());
				case COLUMN_SIZE:
					return compare(a->getSize(), b->getSize());
				case COLUMN_NETWORK_TRAFFIC:
					return compare(a->getActual(), b->getActual());
				case COLUMN_TTH:
					return compare(a->getTTH(), b->getTTH());
				case COLUMN_IP:
					return compare(a->getIP(), b->getIP());
				case COLUMN_DONE:
					return compare(a->getTime(), b->getTime());
				default:
#ifdef _WIN32
					return Util::defaultSort(a->getText(col), b->getText(col));
#else
					return stricmp(a->getText(col), b->getText(col));
#endif
			}
		}
		GETC(string, target, Target);
		GETC(TTHValue, tth, TTH);
		GETC(string, ip, IP);
		GETC(string, nick, Nick);
		GETC(string, hubs, Hubs);
		GETC(string, hub, Hub);
		GETSET(CID, cid, CID);
		GETC(int64_t, size, Size);
		GETC(int64_t, avgSpeed, AvgSpeed);
		GETC(time_t, time, Time);
		GETC(int64_t, actual, Actual); // Socket Bytes!
		GETC(int64_t, id, ID);
		GETSET(int64_t, tempId, TempID);
};

#endif // FINISHED_ITEM_H_

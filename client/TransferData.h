//-----------------------------------------------------------------------------
//(c) 2007-2017 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#ifndef DCPLUSPLUS_DCPP_TRANSFER_DATA_H_
#define DCPLUSPLUS_DCPP_TRANSFER_DATA_H_

#include "HintedUser.h"
#include "TransferFlags.h"
#ifdef _DEBUG
#include "LogManager.h"
#endif

class TransferData
{
	public:
		TransferData():
			type(0), actual(0), pos(0), startPos(0), startTime(0),
			runningAverage(0), secondsLeft(0),/* percent(0),*/
			size(0), fileSize(0), speed(0), transferFlags(0) {}

		uint8_t type;
		int64_t actual;
		int64_t pos;
		int64_t startPos;
		uint64_t startTime;
		int64_t runningAverage;
		int64_t secondsLeft;
		int64_t size;
		int64_t fileSize;
		int64_t speed;
		int transferFlags;
		
		string path;
		string token;
		HintedUser hintedUser;
		
		void dumpToLog() const
		{
#ifdef ____DEBUG
			LogManager::message("TransferData-dump = "
			                    " actual = " + Util::toString(actual) +
			                    " pos = " + Util::toString(pos) +
			                    " startPos = " + Util::toString(startPos) +
			                    " start = " + Util::toString(start) +
			                    " speed = " + Util::toString(speed) +
			                    " secondsLeft = " + Util::toString(secondsLeft) +
			                    " size = " + Util::toString(size) +
			                    " type = " + Util::toString(type) +
			                    " percent = " + Util::toString(percent) +
			                    " user = " + hintedUser.user->getLastNick() +
			                    " hub = " + hintedUser.hint +
			                    " statusString = " + Text::fromT(statusString)
			                   );
#endif
		}
};

typedef std::vector<TransferData> UploadArray;
typedef std::vector<TransferData> DownloadArray;

#endif /*DCPLUSPLUS_DCPP_TRANSFER_DATA_H_*/

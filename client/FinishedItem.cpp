#include "stdinc.h"
#include "FinishedItem.h"
#include "PathUtil.h"
#include "FormatUtil.h"
#include "ResourceManager.h"

tstring FinishedItem::getText(int col) const
{
	dcassert(col >= 0 && col < COLUMN_LAST);
	switch (col)
	{
		case COLUMN_FILE:
			return Text::toT(Util::getFileName(getTarget()));
		case COLUMN_TYPE:
			return Text::toT(Util::getFileExtWithoutDot(getTarget()));
		case COLUMN_DONE:
			return Text::toT(Util::formatDateTime(getTime(), id != 0));
		case COLUMN_PATH:
			return Text::toT(Util::getFilePath(getTarget()));
		case COLUMN_NICK:
			return Text::toT(getNick());
		case COLUMN_HUB:
			return Text::toT(getHubs());
		case COLUMN_SIZE:
			return Util::formatBytesT(getSize());
		case COLUMN_NETWORK_TRAFFIC:
			if (getActual())
				return Util::formatBytesT(getActual());
			else
				return Util::emptyStringT;
		case COLUMN_SPEED:
			if (getAvgSpeed())
				return Util::formatBytesT(getAvgSpeed()) + _T('/') + TSTRING(S);
			else
				return Util::emptyStringT;
		case COLUMN_IP:
			return Text::toT(getIP());
		case COLUMN_TTH:
		{
			if (!getTTH().isZero())
				return Text::toT(getTTH().toBase32());
			else
				return Util::emptyStringT;
		}
		default:
			return Util::emptyStringT;
	}
}

#ifndef STATUS_MESSAGE_HISTORY_H_
#define STATUS_MESSAGE_HISTORY_H_

#include "../client/typedefs.h"
#include "../client/w.h"

class StatusMessageHistory
{
	public:
		StatusMessageHistory(size_t maxCount = 10) : maxCount(maxCount) {}
		void addLine(const tstring& s);
		void getToolTip(NMHDR* hdr) const;
		void clear() { lastLinesList.clear(); }

	private:
		const size_t maxCount;
		std::list<tstring> lastLinesList;
		mutable tstring lastLines;
};

#endif // STATUS_MESSAGE_HISTORY_H_

#include "stdafx.h"
#include "StatusMessageHistory.h"

void StatusMessageHistory::addLine(const tstring& s)
{
	lastLinesList.push_back(s);
	while (lastLinesList.size() > maxCount)
		lastLinesList.pop_front();
}

void StatusMessageHistory::getToolTip(NMHDR* hdr) const
{
	NMTTDISPINFO* nm = reinterpret_cast<NMTTDISPINFO*>(hdr);
	lastLines.clear();
	bool appendNL = false;
	for (auto i = lastLinesList.cbegin(); i != lastLinesList.cend(); ++i)
	{
		if (appendNL) lastLines += _T("\r\n");
		lastLines += *i;
		appendNL = true;
	}
	lastLines.shrink_to_fit();
	nm->lpszText = const_cast<TCHAR*>(lastLines.c_str());
}

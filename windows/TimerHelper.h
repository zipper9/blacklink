#ifndef TIMER_HELPER_H_
#define TIMER_HELPER_H_

#include "../client/w.h"
#include "../client/debug.h"

class TimerHelper
{
	public:
		HWND& hwnd;
		UINT timerID;

		TimerHelper(HWND& hwnd): hwnd(hwnd), timerID(0) {}
		
		~TimerHelper()
		{
			dcassert(timerID == 0);
		}

		bool createTimer(unsigned elapse, unsigned eventID = 1)
		{
			if (timerID) return false;
			dcassert(hwnd != NULL);
			timerID = SetTimer(hwnd, eventID, elapse, nullptr);
			return timerID != 0;
		}

		void destroyTimer()
		{
			if (!timerID) return;
			dcassert(hwnd != NULL);
			KillTimer(hwnd, timerID);
			timerID = 0;
		}

		bool checkTimerID(UINT id) const
		{
			return timerID == id;
		}
};

#endif

#ifndef WIN_EVENT_H_
#define WIN_EVENT_H_

#ifndef _WIN32
#error This file requires Win32
#endif

#include "w.h"
#include "debug.h"

template<BOOL manualReset>
class WinEvent
{
	public:
		WinEvent() noexcept
		{
			handle = NULL;
		}

		~WinEvent() noexcept
		{
			if (handle) CloseHandle(handle);
		}

		WinEvent(const WinEvent& src) = delete;
		WinEvent& operator= (const WinEvent& src) = delete;

		WinEvent(WinEvent&& src) noexcept
		{
			handle = src.handle;
			src.handle = NULL;
		}

		WinEvent& operator= (WinEvent&& src) noexcept
		{
			if (this == &src) return *this;
			if (handle) CloseHandle(handle);
			handle = src.handle;
			src.handle = NULL;
			return *this;
		}

		bool create() noexcept
		{
			if (handle) return true;
			handle = CreateEvent(NULL, manualReset, FALSE, NULL);
			return handle != NULL;
		}
		
		void wait() noexcept
		{
			dcassert(handle);
			#ifdef _DEBUG
			DWORD result = 
			#endif
			WaitForSingleObject(handle, INFINITE);
			#ifdef _DEBUG
			dcassert(result == WAIT_OBJECT_0);
			#endif
		}

		bool timedWait(int msec) noexcept
		{
			dcassert(handle);
			DWORD result = WaitForSingleObject(handle, msec);
			dcassert(result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT);
			return result == WAIT_OBJECT_0;
		}

		void notify() noexcept
		{
			SetEvent(handle);
		}

		void reset() noexcept
		{
			ResetEvent(handle);
		}

		HANDLE getHandle() const { return handle; }
		bool empty() const { return handle == NULL; }

	private:
		HANDLE handle;
};

#endif // WIN_EVENT_H_

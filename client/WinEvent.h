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
			if (handle) CloseHandle(handle);
			handle = src.handle;
			src.handle = NULL;
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
			DWORD result = WaitForSingleObject(handle, INFINITE);
			dcassert(result == WAIT_OBJECT_0);
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

		static int waitMultiple(WinEvent* events, int count)
		{
			static_assert(sizeof(*events) == sizeof(HANDLE), "WinEvent has bad size");
			dcassert(count <= 64);
			DWORD result = WaitForMultipleObjects(count, (HANDLE *) events, FALSE, INFINITE);
			if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + count) return result - WAIT_OBJECT_0;
			return -1;
		}

		static int timedWaitMultiple(WinEvent* events, int count, int msec)
		{
			static_assert(sizeof(*events) == sizeof(HANDLE), "WinEvent has bad size");
			dcassert(count <= 64);
			DWORD result = WaitForMultipleObjects(count, (HANDLE *) events, FALSE, msec);
			if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + count) return result - WAIT_OBJECT_0;
			return -1;
		}

		HANDLE getHandle() const { return handle; }
		bool empty() const { return handle == NULL; }

	private:
		HANDLE handle;
};

#endif // WIN_EVENT_H_

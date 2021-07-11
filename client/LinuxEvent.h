#ifndef LINUX_EVENT_H_
#define LINUX_EVENT_H_

#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <errno.h>
#include "debug.h"

class LinuxEvent
{
	public:
		LinuxEvent() noexcept
		{
			fd = -1;
		}

		~LinuxEvent() noexcept
		{
			if (fd != -1) close(fd);
		}

		LinuxEvent(const LinuxEvent& src) = delete;
		LinuxEvent& operator= (const LinuxEvent& src) = delete;

		LinuxEvent(LinuxEvent&& src) noexcept
		{
			fd = src.fd;
			src.fd = -1;
		}

		LinuxEvent& operator= (LinuxEvent&& src) noexcept
		{
			if (this == &src) return *this;
			if (fd != -1) close(fd);
			fd = src.fd;
			src.fd = -1;
			return *this;
		}

		bool create() noexcept
		{
			if (fd != -1) return true;
			fd = eventfd(0, EFD_NONBLOCK);
			return fd != -1;
		}

		void wait() noexcept
		{
			timedWait(-1);
		}

		bool timedWait(int msec) noexcept
		{
			dcassert(fd != -1);
			pollfd pfd;
			pfd.fd = fd;
			pfd.events = POLLIN;
			pfd.revents = 0;
			while (true)
			{
				int res = poll(&pfd, 1, msec);
				if (res < 0)
				{
					if (errno == EINTR) continue;
					dcassert(0);
				}
				break;
			}
			return (pfd.revents & POLLIN) != 0;
		}

		void notify() noexcept
		{
			dcassert(fd != -1);
			uint64_t val = 1;
			write(fd, &val, sizeof(val));
		}

		void reset() noexcept
		{
			dcassert(fd != -1);
			uint64_t val;
			while (true)
			{
				int res = read(fd, &val, sizeof(val));
				if (res == 0) break;
				if (res < 0 && errno != EINTR) break;
			}
		}

		bool empty() const { return fd == -1; }
		int getHandle() const { return fd; }

	private:
		int fd;
};

#endif // LINUX_EVENT_H_

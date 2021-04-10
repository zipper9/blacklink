#ifndef PIPE_EVENT_H_
#define PIPE_EVENT_H_

#include <unistd.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "debug.h"

class PipeEvent
{
	public:
		PipeEvent() noexcept
		{
			fd[0] = fd[1] = -1;
		}

		~PipeEvent() noexcept
		{
			if (fd[0] != -1) close(fd[0]);
			if (fd[1] != -1) close(fd[1]);
		}

		PipeEvent(const PipeEvent& src) = delete;
		PipeEvent& operator= (const PipeEvent& src) = delete;

		PipeEvent(PipeEvent&& src) noexcept
		{
			for (int i = 0; i < 2; i++)
			{
				fd[i] = src.fd[i];
				src.fd[i] = -1;
			}
		}

		PipeEvent& operator= (PipeEvent&& src) noexcept
		{
			if (this == &src) return *this;
			for (int i = 0; i < 2; i++)
			{
				if (fd[i] != -1) close(fd[i]);
				fd[i] = src.fd[i];
				src.fd[i] = -1;
			}
			return *this;
		}

		bool create() noexcept
		{
			if (fd[0] != -1) return true;
			if (pipe(fd)) return false;
			int nonBlock = 1;
			ioctl(fd[0], FIONBIO, &nonBlock);
			return true;
		}

		void wait() noexcept
		{
			timedWait(-1);
		}

		bool timedWait(int msec) noexcept
		{
			dcassert(fd[0] != -1);
			pollfd pfd;
			pfd.fd = fd[0];
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
			dcassert(fd[1] != -1);
			char val = 1;
			write(fd[1], &val, 1);
		}

		void reset() noexcept
		{
			dcassert(fd[0] != -1);
			char val = 1;
			while (true)
			{
				int res = read(fd[0], &val, 1);
				if (res == 0) break;
				if (res < 0 && errno != EINTR) break;
			}
		}

		bool empty() const { return fd[0] == -1; }
		int getHandle() const { return fd[0]; }

	private:
		int fd[2];
};

#endif // PIPE_EVENT_H_

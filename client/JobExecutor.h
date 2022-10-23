#ifndef JOB_EXECUTOR_H_
#define JOB_EXECUTOR_H_

#include "Thread.h"
#include "Locks.h"
#include "WaitableEvent.h"

class JobExecutor : public Thread
{
	public:
		class Job
		{
			public:
				virtual ~Job() {}
				virtual void run() = 0;
		};
	
	public:
		JobExecutor() : shutdownFlag(false), runningFlag(false), maxSleepTime(15000) {}

		~JobExecutor()
		{
			dcassert(!runningFlag);
		}

		bool addJob(Job* job) noexcept;
		void shutdown(bool finishJob = false) noexcept;

		void setMaxSleepTime(int milliseconds) noexcept
		{
			cs.lock();
			maxSleepTime = milliseconds;
			cs.unlock();
		}

		bool isRunning() const noexcept
		{
			cs.lock();
			bool result = runningFlag;
			cs.unlock();
			return result;
		}

	protected:
		virtual int run() noexcept override;

	private:
		std::list<Job*> jobs;
		mutable CriticalSection cs;
		WaitableEvent event;
		bool shutdownFlag;
		bool runningFlag;
		int maxSleepTime;
};

#endif // JOB_EXECUTOR_H_

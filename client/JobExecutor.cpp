#include "stdinc.h"
#include "JobExecutor.h"

bool JobExecutor::addJob(JobExecutor::Job *job) noexcept
{			
	cs.lock();
	if (shutdownFlag)
	{
		cs.unlock();
		return false;
	}
	event.create();
	bool isEmpty = jobs.empty();
	bool isRunning = true;
	jobs.push_back(job);
	std::swap(runningFlag, isRunning);
	cs.unlock();
	if (!isRunning)
	{
		if (threadHandle != INVALID_THREAD_HANDLE)
		{
			BaseThread::closeHandle(threadHandle);
			threadHandle = INVALID_THREAD_HANDLE;
		}	
		start(0, "JobExecutor");
	}
	if (isEmpty) event.notify();
	return true;
}

void JobExecutor::shutdown() noexcept
{
	Handle savedHandle = INVALID_THREAD_HANDLE;
	std::list<Job*> savedJobs;
	cs.lock();
	std::swap(jobs, savedJobs);
	std::swap(threadHandle, savedHandle);
	shutdownFlag = true;		
	if (!event.empty()) event.notify();
	cs.unlock();
	if (savedHandle != INVALID_THREAD_HANDLE)
		BaseThread::join(savedHandle, INFINITE_TIMEOUT);
	for (auto i = savedJobs.begin(); i != savedJobs.end(); i++)
		delete *i;
}

int JobExecutor::run() noexcept
{
	bool timedOut = false;
	for (;;)
	{
		Job* job = nullptr;
		int waitTime;
		cs.lock();
		if (shutdownFlag)
		{
			runningFlag = false;
			cs.unlock();
			break;
		}			
		waitTime = maxSleepTime;
		if (jobs.empty())
		{
			if (timedOut)
			{
				runningFlag = false;
				cs.unlock();
				break;
			}
		}
		else
		{
			job = jobs.front();
			jobs.pop_front();
		}
		cs.unlock();
		if (job)
		{
			job->run();
			delete job;
			continue;
		}
		timedOut = !event.timedWait(waitTime);
	}
	return 0;
}

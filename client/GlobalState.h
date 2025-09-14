#ifndef GLOBAL_STATE_H_
#define GLOBAL_STATE_H_

namespace GlobalState
{
	void shutdown();
	void prepareShutdown();
	bool isShuttingDown();
	bool isShutdown();

	void started();
	bool isStartingUp();
}

#endif // GLOBAL_STATE_H_

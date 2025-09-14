#include "stdinc.h"
#include "GlobalState.h"
#include "TimerManager.h"

static std::atomic_bool startingUp = true;

static std::atomic_bool shutDown = false;
static std::atomic_bool shuttingDown = false;

void GlobalState::shutdown()
{
	dcassert(!shutDown);
	shutDown = true;
	shuttingDown = true;
}

void GlobalState::prepareShutdown()
{
	dcassert(!shuttingDown);
	shuttingDown = true;
	TimerManager::getInstance()->setTicksDisabled(true);
}

bool GlobalState::isShuttingDown()
{
	return shuttingDown;
}

bool GlobalState::isShutdown()
{
	return shutDown;
}

void GlobalState::started()
{
	startingUp = false;
}

bool GlobalState::isStartingUp()
{
	return startingUp;
}

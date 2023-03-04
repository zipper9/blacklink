#ifndef CONNECTION_STATUS_H_
#define CONNECTION_STATUS_H_

#include <time.h>

struct ConnectionStatus
{
	enum Status
	{
		UNKNOWN = -1,
		SUCCESS,
		FAILURE,
		CONNECTING // used by FavoriteManager::changeConnectionStatus
	};

	ConnectionStatus() : status(UNKNOWN), lastAttempt(0), lastSuccess(0) {}

	Status status;
	time_t lastAttempt;
	time_t lastSuccess;
};

#endif

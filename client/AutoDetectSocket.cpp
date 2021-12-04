#include "stdinc.h"
#include "AutoDetectSocket.h"
#include "BaseThread.h"

static const int POLL_TIME = 10;

bool AutoDetectSocket::waitAccepted(unsigned millis)
{
	if (type == TYPE_TCP)
		return Socket::waitAccepted(millis);
	if (!Socket::waitAccepted(millis))
		return false;
	uint8_t buf[8];
	for (;;)
	{
		int size = ::recv(sock, (char *) buf, sizeof(buf), MSG_PEEK);
		if (size >= 8) break;
		if (millis < POLL_TIME) return false;
		millis -= POLL_TIME;
		BaseThread::sleep(POLL_TIME);
	}	
	if (buf[0] == 22 && buf[1] == 3 && buf[6] == 0)
		type = TYPE_SSL;
	else
		type = TYPE_TCP;	
	return true;
}

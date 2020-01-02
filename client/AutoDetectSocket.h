#ifndef AUTODETECT_SOCKET_H_
#define AUTODETECT_SOCKET_H_

#include "Socket.h"

class AutoDetectSocket : public Socket
{
	public:
		enum
		{
			TYPE_UNKNOWN,
			TYPE_TCP,
			TYPE_SSL
		};

		AutoDetectSocket() {}
		virtual bool waitAccepted(uint64_t millis) override;

		virtual SecureTransport getSecureTransport() const noexcept override
		{
			return SECURE_TRANSPORT_DETECT;
		}

		int getType() const
		{
			return type;
		}

	private:
		int type = TYPE_UNKNOWN;
};

#endif // AUTODETECT_SOCKET_H_

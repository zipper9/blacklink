#ifndef WEB_SERVER_LISTENER_H_
#define WEB_SERVER_LISTENER_H_

#include "noexcept.h"

class WebServerListener
{
	public:
		template<int I> struct X
		{
			enum { TYPE = I };
		};

		typedef X<0> ShutdownPC;

		virtual void on(ShutdownPC, int action) noexcept = 0;
};

#endif

#ifndef USER_MANAGER_LISTENER_H_
#define USER_MANAGER_LISTENER_H_

#include "forward.h"
#include "typedefs.h"
#include "noexcept.h"

class UserManagerListener
{
	public:
		virtual ~UserManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> OutgoingPrivateMessage;
		typedef X<1> OpenHub;
		typedef X<2> IgnoreListChanged;
		typedef X<3> IgnoreListCleared;
		typedef X<4> ReservedSlotChanged;
		
		virtual void on(OutgoingPrivateMessage, const UserPtr&, const string&, const tstring&) noexcept { }
		virtual void on(OpenHub, const string&, const UserPtr&) noexcept { }
		virtual void on(IgnoreListChanged) noexcept { }
		virtual void on(IgnoreListCleared) noexcept { }
		virtual void on(ReservedSlotChanged, const UserPtr&) noexcept { }
};

#endif // USER_MANAGER_LISTENER_H_

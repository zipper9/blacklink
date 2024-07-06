#ifndef DCPLUSPLUS_DCPP_UPLOADMANAGERLISTENER_H_
#define DCPLUSPLUS_DCPP_UPLOADMANAGERLISTENER_H_

#include "typedefs.h"
#include "forward.h"
#include "noexcept.h"
#include "TransferData.h"
#include "Upload.h"

class UploadManagerListener
{
	public:
		virtual ~UploadManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};

		typedef X<0> Complete;
		typedef X<1> Failed;
		typedef X<2> Starting;
		typedef X<3> Tick;
		typedef X<4> QueueAdd;
		typedef X<5> QueueRemove;
		
		virtual void on(Starting, const UploadPtr&) noexcept { }
		virtual void on(Tick, const UploadArray&) noexcept {}
		virtual void on(Complete, const UploadPtr&) noexcept { }
		virtual void on(Failed, const UploadPtr&, const string&) noexcept { }
		virtual void on(QueueAdd, const HintedUser&, const UploadQueueFilePtr&) noexcept { }
		virtual void on(QueueRemove, const UserPtr&) noexcept { }
};

#endif /*UPLOADMANAGERLISTENER_H_*/

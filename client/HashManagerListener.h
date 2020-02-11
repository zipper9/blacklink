#ifndef HASH_MANAGER_LISTENER_H_
#define HASH_MANAGER_LISTENER_H_

#include "SharedFile.h"

class HashManagerListener
{
	public:
		virtual ~HashManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> TTHDone;
		
		virtual void on(TTHDone, int64_t fileID, const SharedFilePtr& file, const string& fileName, const TTHValue& root, int64_t size) noexcept = 0;
};

#endif // HASH_MANAGER_LISTENER_H_

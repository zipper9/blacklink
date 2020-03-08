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
		
		typedef X<0> FileHashed;
		typedef X<1> HashingError;
		typedef X<1> HashingAborted;
		
		virtual void on(FileHashed, int64_t fileID, const SharedFilePtr& file, const string& fileName, const TTHValue& root, int64_t size) noexcept = 0;
		virtual void on(HashingError, int64_t fileID, const SharedFilePtr& file, const string& fileName) noexcept = 0;
		virtual void on(HashingAborted) noexcept = 0;
};

#endif // HASH_MANAGER_LISTENER_H_

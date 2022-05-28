#ifndef TAG_COLLECTOR_H_
#define TAG_COLLECTOR_H_

#include "Locks.h"
#include "typedefs.h"

class TagCollector
{
		static const size_t MAX_TAGS = 256;
		static const size_t MAX_SOURCES = 10;
		struct TagData
		{
			string sources[MAX_SOURCES];
			size_t count = 0;
		};

		boost::unordered_map<string, TagData> data;
		mutable CriticalSection csData;

	public:
		bool addTag(const string& tag, const string& source);
		void clear();
		string getInfo() const;
};

#endif // TAG_COLLECTOR_H_

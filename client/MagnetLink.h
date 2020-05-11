#ifndef MAGNET_LINK_H_
#define MAGNET_LINK_H_

#include "typedefs.h"

class MagnetLink
{
	public:
		bool parse(const string& url);
		bool isValid() const;
		void clear();
		const char* getTTH() const;
		const string& getFileName() const;

		std::list<string> exactTopic;
		string displayName;
		string exactSource;
		string acceptableSource;
		string keywordTopic;
		int64_t exactLength = -1;
		int64_t dirSize = -1;
};

#endif // MAGNET_LINK_H_

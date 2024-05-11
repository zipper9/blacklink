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
		const char* getSHA1(int *hashSize = nullptr) const;
		const char* getSHA256() const;
		const string& getFileName() const;
		bool isBitTorrent() const { return getSHA1() || getSHA256(); }

		std::list<string> exactTopic;
		string displayName;
		string exactSource;
		string acceptableSource;
		string keywordTopic;
		int64_t exactLength = -1;
		int64_t dirSize = -1;
};

#endif // MAGNET_LINK_H_

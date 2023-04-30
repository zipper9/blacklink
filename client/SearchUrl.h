#ifndef SEARCH_URL_H_
#define SEARCH_URL_H_

#include "typedefs.h"

class SearchUrl
{
	public:
		enum Type
		{
			KEYWORD,
			HOSTNAME,
			IP4,
			IP6,
			MAX_TYPE = IP6
		};

		typedef vector<SearchUrl> List;

		string url;
		string description;
		Type type;
};

#endif

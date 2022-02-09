#ifndef SEARCH_HISTORY_H_
#define SEARCH_HISTORY_H_

#include "../client/DatabaseManager.h"

class SearchHistory
{
	private:
		std::list<tstring> data;
		bool firstLoadFlag = true;

	public:
		const std::list<tstring>& getData() const { return data; }
		bool empty() const { return data.empty(); }
		void addItem(const tstring& s, int maxCount);
		void load(DBRegistryType type);
		void firstLoad(DBRegistryType type);
		void save(DBRegistryType type);
		void clear(DBRegistryType type);
};

#endif // SEARCH_HISTORY_H_

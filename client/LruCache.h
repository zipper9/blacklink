#ifndef LRU_CACHE_H_
#define LRU_CACHE_H_

#include "typedefs.h"

template<typename item_type, typename key_type>
class LruCache
{
	public:
		typedef item_type Item;
		typedef key_type Key;

		bool add(const Item& item, Item **storedItem = nullptr)
		{
			auto p = items.insert(item);
			if (!p.second)
			{
				if (storedItem) *storedItem = const_cast<Item*>(&(*p.first));
				return false;
			}
			Item* newItem = const_cast<Item*>(&(*p.first));
			newItem->next = nullptr;
			if (newestItem)
				newestItem->next = newItem;
			else
				oldestItem = newItem;
			newestItem = newItem;
			if (storedItem) *storedItem = newItem;
			return true;	
		}
		
		const Item* get(const Key& key) const
		{
			Item tempItem;
			tempItem.key = key;
			auto i = items.find(tempItem);
			return i != items.cend() ? &(*i) : nullptr;
		}
		
		void clear()
		{
			items.clear();
			oldestItem = newestItem = nullptr;
		}
		
		void removeOldest(size_t sizeThreshold)
		{
			while (items.size() >= sizeThreshold && removeOldest());
		}
		
		bool removeOldest()
		{
			if (!oldestItem) return false;
			Item* nextItem = oldestItem->next;
			items.erase(*oldestItem);
			oldestItem = nextItem;
			if (!oldestItem) newestItem = nullptr;
			return true;
		}

#ifdef _DEBUG
		const Item* getOldestItem() const { return oldestItem; }
		const Item* getNewestItem() const { return newestItem; }
#endif

	private:
		struct ItemTraits
		{
			size_t operator()(const Item& item) const
			{
				return boost::hash<key_type>()(item.key);
			}

			bool operator()(const Item& a, const Item& b) const
			{
				return a.key == b.key;
			}
		};

		boost::unordered_set<item_type, ItemTraits, ItemTraits> items;
		Item* oldestItem = nullptr;
		Item* newestItem = nullptr;
};


#endif // LRU_CACHE_H_

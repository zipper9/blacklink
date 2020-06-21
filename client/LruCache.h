#ifndef LRU_CACHE_H_
#define LRU_CACHE_H_

#include "typedefs.h"

template<typename item_type, typename key_type>
class LruCache
{
	public:
		typedef item_type Item;
		typedef key_type Key;

		~LruCache()
		{
			clear();
		}

		bool add(const Item& item, Item **storedItem = nullptr)
		{
			auto p = items.insert(make_pair(item.key, item));
			if (!p.second)
			{
				if (storedItem) *storedItem = &p.first->second;
				return false;
			}
			Item* newItem = &p.first->second;
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
			auto i = items.find(key);
			return i != items.cend() ? &i->second : nullptr;
		}
		
		Item* get(const Key& key)
		{
			auto i = items.find(key);
			return i != items.end() ? &i->second : nullptr;
		}

		void clear()
		{
			if (deleter)
			{
				for (auto& p : items)
					deleter(p.second);
			}
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
			auto p = items.find(oldestItem->key);
			if (p != items.end())
			{
				if (deleter) deleter(p->second);
				items.erase(p);
			}
			oldestItem = nextItem;
			if (!oldestItem) newestItem = nullptr;
			return true;
		}

		void setDeleter(void (*func)(Item&))
		{
			deleter = func;
		}

#ifdef _DEBUG
		const Item* getOldestItem() const { return oldestItem; }
		const Item* getNewestItem() const { return newestItem; }
#endif

	private:
		boost::unordered_map<key_type, item_type> items;
		Item* oldestItem = nullptr;
		Item* newestItem = nullptr;
		void (*deleter)(Item&) = nullptr;
};

template<typename item_type, typename key_type>
class LruCacheEx
{
	public:
		typedef item_type Item;
		typedef key_type Key;

		bool add(const Item& item, Item **storedItem = nullptr)
		{
			auto p = items.insert(make_pair(item.key, item));
			if (!p.second)
			{
				if (storedItem) *storedItem = &p.first->second;
				return false;
			}
			Item* newItem = &p.first->second;
			newItem->next = nullptr;
			if (newestItem)
				newestItem->next = newItem;
			else
				oldestItem = newItem;
			newItem->prev = newestItem;
			newestItem = newItem;
			if (storedItem) *storedItem = newItem;
			return true;	
		}
		
		const Item* get(const Key& key) const
		{
			auto i = items.find(key);
			return i != items.cend() ? &i->second : nullptr;
		}
		
		Item* get(const Key& key)
		{
			auto i = items.find(key);
			return i != items.end() ? &i->second : nullptr;
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
			auto p = items.find(oldestItem->key);
			if (p != items.end()) items.erase(p);
			oldestItem = nextItem;
			if (oldestItem)
				oldestItem->prev = nullptr;
			else
				newestItem = nullptr;
			return true;
		}

		void makeNewest(Item* item)
		{
			if (!item->next) return;
			dcassert(newestItem);
			item->next->prev = item->prev;
			if (item->prev)
				item->prev->next = item->next;
			else
				oldestItem = item->next;
			newestItem->next = item;
			item->prev = newestItem;
			item->next = nullptr;
			newestItem = item;
		}

#ifdef _DEBUG
		const Item* getOldestItem() const { return oldestItem; }
		const Item* getNewestItem() const { return newestItem; }
#endif

	private:
		boost::unordered_map<key_type, item_type> items;
		Item* oldestItem = nullptr;
		Item* newestItem = nullptr;
};

#endif // LRU_CACHE_H_

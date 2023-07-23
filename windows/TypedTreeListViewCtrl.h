#ifndef TYPED_TREE_LIST_VIEW_CTRL_H_
#define TYPED_TREE_LIST_VIEW_CTRL_H_

#include "TypedListViewCtrl.h"
#include "LockRedraw.h"
#include "../client/BusyCounter.h"

// Copyright (C) 2005-2009 Big Muscle, StrongDC++
template<class T, int ctrlId, class KValue>
class TypedTreeListViewCtrl : public TypedListViewCtrl<T, ctrlId>
{
	public:
		enum
		{
			STATE_FLAG_GROUP    = 1,
			STATE_FLAG_VISIBLE  = 2, // TODO
			STATE_FLAG_EXPANDED = 4
		};

		TypedTreeListViewCtrl() : uniqueParent(false)
		{
		}
		~TypedTreeListViewCtrl()
		{
			states.Destroy();
		}
		
		typedef TypedTreeListViewCtrl<T, ctrlId, KValue> thisClass;
		typedef TypedListViewCtrl<T, ctrlId> baseClass;
		
		struct ParentPair
		{
			T* parent;
			vector<T*> children;
		};
		
		typedef std::pair<KValue, ParentPair> ParentMapPair;
		typedef std::unordered_map<KValue, ParentPair> ParentMap;

		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButton)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP();
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			ResourceLoader::LoadImageList(IDR_STATE, states, 16, 16);
			SetImageList(states, LVSIL_STATE);
			
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
			CPoint pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			
			LVHITTESTINFO lvhti;
			lvhti.pt = pt;
			
			int pos = SubItemHitTest(&lvhti);
			if (pos != -1)
			{
				CRect rect;
				GetItemRect(pos, rect, LVIR_ICON);
				if (pt.x < rect.left)
				{
					T* i = getItemData(pos);
					if (i->parent == NULL)
					{
						if (i->getStateFlags() & STATE_FLAG_EXPANDED)
							Collapse(i, pos);
						else
							Expand(i, pos);
					}
				}
			}
			
			bHandled = FALSE;
			return 0;
		}
		
		void Collapse(T* parent, int itemPos)
		{
			SetRedraw(FALSE);
			const vector<T*>& children = findChildren(parent->getGroupCond());
			for (auto i = children.cbegin(); i != children.cend(); ++i)
			{
				deleteItem(*i);
			}
			parent->setStateFlags(parent->getStateFlags() & ~STATE_FLAG_EXPANDED);
			SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			SetRedraw(TRUE);
		}
		
		void Expand(T* parent, int itemPos)
		{
			SetRedraw(FALSE);
			const vector<T*>& children = findChildren(parent->getGroupCond());
			if (children.size() > (size_t)(uniqueParent ? 1 : 0))
			{
				parent->setStateFlags(parent->getStateFlags() | STATE_FLAG_EXPANDED);
				for (auto i = children.cbegin(); i != children.cend(); ++i)
				{
					insertChild(*i, itemPos + 1);
				}
				SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
				resort();
			}
			SetRedraw(TRUE);
		}
		
		void insertChild(const T* item, int idx)
		{
			LVITEM lvi;
			lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE/* | LVIF_INDENT*/;
			lvi.iItem = idx;
			lvi.iSubItem = 0;
			#if 0
			lvi.iIndent = 1;
			#endif
			lvi.pszText = LPSTR_TEXTCALLBACK;
			lvi.iImage = item->getImageIndex();
			lvi.lParam = (LPARAM)item;
			lvi.state = 0;
			lvi.stateMask = 0;
			InsertItem(&lvi);
		}

		const vector<T*>& findChildren(const KValue& groupCond) const
		{
			static const vector<T*> emptyVector;
			dcassert(!destroyingItems);
			ParentMap::const_iterator i = parents.find(groupCond);
			if (i != parents.end())
				return i->second.children;
			return emptyVector;
		}
		
		ParentPair* findParentPair(const KValue& groupCond)
		{
			ParentMap::iterator i = parents.find(groupCond);
			if (i != parents.end())
				return &i->second;
			return nullptr;
		}
		
		void changeGroupCondNonVisual(T* item, const KValue& newGroupCond)
		{
			dcassert(!item->parent);
			if (!(item->getStateFlags() & STATE_FLAG_GROUP)) return;
			auto it = parents.find(item->getGroupCond());
			dcassert(it != parents.end());
			if (it != parents.end())
			{
				ParentPair pp = std::move(it->second);
				parents.erase(it);
				parents.insert(ParentMapPair(newGroupCond, pp));
			}
		}
		
		int insertChildNonVisual(T* item, ParentPair* pp, bool autoExpand, bool useVisual, bool useImageCallback)
		{
			dcassert(!destroyingItems);
			T* parent = nullptr;
			int pos = -1;
			if (pp->children.empty())
			{
				T* oldParent = pp->parent;
				parent = oldParent->createParent();
				if (parent != oldParent)
				{
					uniqueParent = true;
					parents.erase(oldParent->getGroupCond());
					deleteItem(oldParent);
					
					ParentPair newPP = { parent };
					pp = &(parents.insert(ParentMapPair(parent->getGroupCond(), newPP)).first->second);
					
					parent->parent = nullptr; // ensure that parent of this item is really NULL
					oldParent->parent = parent;
					pp->children.push_back(oldParent); // mark old parent item as a child
					parent->hits++;
					if (useVisual)
						pos = baseClass::insertItem(getSortPos(parent), parent, useImageCallback ? I_IMAGECALLBACK : parent->getImageIndex());
				}
				else
				{
					uniqueParent = false;
					if (useVisual)
						pos = findItem(parent);
				}
				
				if (pos != -1)
				{
					if (autoExpand)
					{
						if (useVisual)
							SetItemState(pos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
						parent->setStateFlags(parent->getStateFlags() | STATE_FLAG_EXPANDED);
					}
					else if (useVisual)
						SetItemState(pos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
				}
			}
			else
			{
				parent = pp->parent;
				if (useVisual)
					pos = findItem(parent);
			}

			pp->children.push_back(item);
			parent->hits++;
			item->parent = parent;
			if (pos != -1 && useVisual)
			{
				if (parent->getStateFlags() & STATE_FLAG_EXPANDED)
					insertChild(item, pos + static_cast<int>(pp->children.size()));
				updateItem(pos);
			}

			return pos;
		}
		
		int insertGroupedItem(T* item, bool autoExpand, bool useImageCallback)
		{
			T* parent = nullptr;
			item->setStateFlags(item->getStateFlags() | STATE_FLAG_GROUP);
			ParentPair* pp = findParentPair(item->getGroupCond());
			int pos = -1;
			if (pp == nullptr)
			{
				parent = item;
				
				ParentPair newPP = { parent };
				dcassert(!destroyingItems);
				parents.insert(ParentMapPair(parent->getGroupCond(), newPP));
				
				parent->parent = nullptr; // ensure that parent of this item is really NULL
				pos = baseClass::insertItem(getSortPos(parent), parent, useImageCallback ? I_IMAGECALLBACK : parent->getImageIndex());
				return pos;
			}
			else
			{
				pos = insertChildNonVisual(item, pp, autoExpand, true, useImageCallback);
			}
			return pos;
		}

		int insertItem(T* item, int image)
		{
			item->setStateFlags(item->getStateFlags() & ~STATE_FLAG_GROUP);
			item->parent = nullptr;
			return baseClass::insertItem(getSortPos(item), item, image);
		}

		int insertItem(int i, T* item, int image)
		{
			item->setStateFlags(item->getStateFlags() & ~STATE_FLAG_GROUP);
			item->parent = nullptr;
			return baseClass::insertItem(i, item, image);
		}

		void removeParent(T* parent)
		{
			dcassert(!destroyingItems);
			BusyCounter<bool> busy(destroyingItems);
			ParentPair* pp = findParentPair(parent->getGroupCond());
			if (pp)
			{
				for (auto i = pp->children.cbegin(); i != pp->children.cend(); ++i)
				{
					deleteItem(*i);
					if (ownsItemData)
						delete *i;
				}
				pp->children.clear();
				parents.erase(parent->getGroupCond());
			}
			deleteItem(parent);
		}
		
		void removeGroupedItem(T* item, bool removeFromMemory = true)
		{
			if (!item->parent)
			{
				if (item->getStateFlags() & STATE_FLAG_GROUP)
					removeParent(item);
				else
					deleteItem(item);
			}
			else
			{
				dcassert(!destroyingItems);
				BusyCounter<bool> busy(destroyingItems);
				T* parent = item->parent;
				ParentPair* pp = findParentPair(parent->getGroupCond());

				// TODO: delete item only if it's visible
				deleteItem(item);
				if (pp)
				{
					const auto n = find(pp->children.begin(), pp->children.end(), item);
					if (n != pp->children.end())
					{
						pp->children.erase(n);
						pp->parent->hits--;
					}
					if (uniqueParent)
					{
						dcassert(!pp->children.empty());
						if (pp->children.size() == 1)
						{
							const T* oldParent = parent;
							parent = pp->children.front();
							
							deleteItem(oldParent);
							parents.erase(oldParent->getGroupCond());
							if (ownsItemData)
								delete oldParent;
								
							ParentPair newPP = { parent };
							parents.insert(ParentMapPair(parent->getGroupCond(), newPP));
							
							parent->parent = nullptr; // ensure that parent of this item is really NULL
							deleteItem(parent);
							baseClass::insertItem(getSortPos(parent), parent, parent->getImageIndex());
						}
					}
					else
					{
						if (pp->children.empty())
							SetItemState(findItem(parent), INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
					}
				}
				updateItem(parent);
			}
			
			if (removeFromMemory && ownsItemData)
				delete item;
		}
		
		void deleteAllNoLock()
		{
			// HACK: ugly hack but at least it doesn't crash and there's no memory leak
			DeleteAllItems();
			if (ownsItemData)
				for (auto i = parents.cbegin(); i != parents.cend(); ++i)
				{
					T* ti = i->second.parent;
					for (auto j : i->second.children)
						delete j;
					delete ti;
				}
			parents.clear();
		}

		void deleteAll()
		{
			dcassert(!destroyingItems);
			BusyCounter<bool> busy(destroyingItems);
			CLockRedraw<> lockRedraw(m_hWnd);
			deleteAllNoLock();
		}

		int getSortPos(const T* a)
		{
			int high = GetItemCount();
			int sortColumn = getRealSortColumn();
			if (sortColumn == -1 || high == 0)
				return high;
				
			high--;
			
			int low = 0;
			int mid = 0;
			T* b = nullptr;
			int comp = 0;
			while (low <= high)
			{
				mid = (low + high) / 2;
				b = getItemData(mid);
				comp = compareItems(a, b, static_cast<uint8_t>(sortColumn));
				
				if (comp == 0)
					return mid;

				if (!isAscending())
					comp = -comp;
					
				if (comp < 0)
					high = mid - 1;
				else
					low = mid + 1;
			}
			
			comp = compareItems(a, b, static_cast<uint8_t>(sortColumn));
			if (!isAscending())
				comp = -comp;
			if (comp > 0)
				mid++;
				
			return mid;
		}
		ParentMap& getParents()
		{
			return parents;
		}

	protected:
		void sortItems() override
		{
			SortItems(&compareFunc, reinterpret_cast<LPARAM>(this));
		}

	private:
		/** map of all parent items with their associated children */
		ParentMap parents;
		/** +/- images */
		CImageList states;
		
		/** is extra item needed for parent items? */
		bool uniqueParent;
		
		static int CALLBACK compareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
		{
			thisClass* t = (thisClass*)lParamSort;
			int result = compareItems((T*)lParam1, (T*)lParam2, t->getRealSortColumn());
			
			if (result == 2)
				result = (t->isAscending() ? 1 : -1);
			else if (result == -2)
				result = (t->isAscending() ? -1 : 1);
				
			return (t->isAscending() ? result : -result);
		}
		
		static int compareItems(const T* a, const T* b, uint8_t col)
		{
			// Copyright (C) Liny, RevConnect
			
			// both are children
			if (a->parent && b->parent)
			{
				// different parent
				if (a->parent != b->parent)
					return compareItems(a->parent, b->parent, col);
			}
			else
			{
				if (a->parent == b)
					return 2;  // a should be displayed below b
					
				if (b->parent == a)
					return -2; // b should be displayed below a
					
				if (a->parent)
					return compareItems(a->parent, b, col);
					
				if (b->parent)
					return compareItems(a, b->parent, col);
			}
			
			return T::compareItems(a, b, col);
		}
};

#endif // TYPED_TREE_LIST_VIEW_CTRL_H_

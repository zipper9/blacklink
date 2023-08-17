#ifndef TYPED_TREE_LIST_VIEW_CTRL_H_
#define TYPED_TREE_LIST_VIEW_CTRL_H_

#include "TypedListViewCtrl.h"
#include "LockRedraw.h"
#include "../client/BusyCounter.h"

// Copyright (C) 2005-2009 Big Muscle, StrongDC++
template<class T, int ctrlId, class KValue, bool uniqueParent>
class TypedTreeListViewCtrl : public TypedListViewCtrl<T, ctrlId>
{
	// uniqueParent == true:
	//     T::createParent will be called to create a new parent
	// uniqueParent == false:
	//     the first item in group becomes the new parent,
	//     T::createParent is not used
	public:
		enum
		{
			STATE_FLAG_VISIBLE            = 1,
			STATE_FLAG_EXPANDED           = 2,
			STATE_FLAG_USE_IMAGE_CALLBACK = 4
		};

		~TypedTreeListViewCtrl()
		{
			states.Destroy();
		}

		typedef TypedTreeListViewCtrl<T, ctrlId, KValue, uniqueParent> thisClass;
		typedef TypedListViewCtrl<T, ctrlId> baseClass;

		struct GroupInfo
		{
			T* parent;
			vector<T*> children;
		};

		typedef std::shared_ptr<GroupInfo> GroupInfoPtr;
		typedef boost::unordered_map<KValue, GroupInfoPtr> GroupMap;

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
					if (i->groupInfo && i->groupInfo->parent == i)
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
			if (parent->groupInfo)
			{
				const vector<T*>& children = parent->groupInfo->children;
				for (T* item : children)
				{
					if (deleteItem(item) != -1)
						item->setStateFlags(item->getStateFlags() & ~STATE_FLAG_VISIBLE);
				}
			}
			parent->setStateFlags(parent->getStateFlags() & ~STATE_FLAG_EXPANDED);
			SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			SetRedraw(TRUE);
		}

		void Expand(T* parent, int itemPos)
		{
			SetRedraw(FALSE);
			if (parent->groupInfo && !parent->groupInfo->children.empty())
			{
				parent->setStateFlags(parent->getStateFlags() | STATE_FLAG_EXPANDED);
				const vector<T*>& children = parent->groupInfo->children;
				for (T* item : children)
					if (!(item->getStateFlags() & STATE_FLAG_VISIBLE))
					{
						insertChild(itemPos + 1, item);
						item->setStateFlags(item->getStateFlags() | STATE_FLAG_VISIBLE);
					}
				SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
				resort();
			}
			SetRedraw(TRUE);
		}

		void changeGroupCondNonVisual(T* item, const KValue& newGroupCond)
		{
			if (!item->groupInfo) return;
			auto it = groups.find(item->getGroupCond());
			if (it != groups.end())
			{
				auto gi = it->second;
				dcassert(!gi->parent);
				groups.erase(it);
				groups.insert(make_pair(newGroupCond, gi));
			}
		}

		int insertGroupedItem(T* item, bool autoExpand, bool useImageCallback)
		{
			dcassert(!item->groupInfo);
			item->hits = -1;
			GroupInfoPtr gi;
			auto it = groups.find(item->getGroupCond());
			if (it == groups.end())
			{
				gi = std::make_shared<GroupInfo>();
				gi->parent = nullptr;
				groups.insert(std::make_pair(item->getGroupCond(), gi));
			}
			else
				gi = it->second;
			item->groupInfo = gi;
			gi->children.push_back(item);

			T* parent = gi->parent;
			if (!parent && gi->children.size() > 1)
			{
				if (uniqueParent)
				{
					parent = item->createParent();
					dcassert(parent);
					parent->groupInfo = gi;
				}
				else
				{
					parent = gi->children.front();
					gi->children.erase(gi->children.begin());
					deleteItem(parent);
				}
				parent->hits = gi->children.size();
				gi->parent = parent;
				int parentPos = baseClass::insertItem(getSortPos(parent), parent, useImageCallback ? I_IMAGECALLBACK : parent->getImageIndex());
				if (autoExpand)
				{
					bool resortFlag = false;
					parent->setStateFlags(parent->getStateFlags() | STATE_FLAG_EXPANDED);
					for (T* child : gi->children)
						if (!(child->getStateFlags() & STATE_FLAG_VISIBLE))
						{
							insertChild(parentPos + 1, child);
							child->setStateFlags(item->getStateFlags() | STATE_FLAG_VISIBLE);
							resortFlag = true;
						}
						else
							updateChild(child);
					SetItemState(parentPos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
					if (resortFlag) resort();
				}
				else
				{
					parent->setStateFlags(parent->getStateFlags() & ~STATE_FLAG_EXPANDED);
					for (T* child : gi->children)
						if (child->getStateFlags() & STATE_FLAG_VISIBLE)
						{
							deleteItem(child);
							child->setStateFlags(child->getStateFlags() & ~STATE_FLAG_VISIBLE);
						}
					SetItemState(parentPos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
				}
				return parentPos;
			}

			if (parent)
			{
				if (parent->getStateFlags() & STATE_FLAG_EXPANDED)
					return insertItemInternal(getSortPos(item), item, useImageCallback ? I_IMAGECALLBACK : item->getImageIndex());
				item->setStateFlags(item->getStateFlags() & ~STATE_FLAG_VISIBLE);
				parent->hits++;
				return -1;
			}

			return insertItemInternal(getSortPos(item), item, useImageCallback ? I_IMAGECALLBACK : item->getImageIndex());
		}

		int insertItem(T* item, int image)
		{
			dcassert(!item->groupInfo);
			return insertItemInternal(getSortPos(item), item, image);
		}

		int insertItem(int i, T* item, int image)
		{
			dcassert(!item->groupInfo);
			return insertItemInternal(i, item, image);
		}

		void removeGroupedItem(T* item, bool removeFromMemory = true)
		{
			bool resortFlag = false;
			GroupInfoPtr& gi = item->groupInfo;
			if (gi)
			{
				auto& children = gi->children;
				if (gi->parent == item)
				{
					for (T* child : children)
						if (child->getStateFlags() & STATE_FLAG_VISIBLE)
						{
							deleteItem(child);
							if (ownsItemData) delete child;
						}
					deleteItem(item);
					groups.erase(item->getGroupCond());
				}
				else
				{
					if (item->getStateFlags() & STATE_FLAG_VISIBLE) deleteItem(item);
					auto it = std::find(children.begin(), children.end(), item);
					if (it != children.end())
					{
						children.erase(it);
						if (uniqueParent && children.size() <= 1)
						{
							deleteItem(gi->parent);
							if (ownsItemData) delete gi->parent;
							gi->parent = nullptr;
							if (!children.empty())
							{
								resortFlag = true;
								for (T* child : children)
									if (!(child->getStateFlags() & STATE_FLAG_VISIBLE))
									{
										insertChild(0, child);
										child->setStateFlags(item->getStateFlags() | STATE_FLAG_VISIBLE);
									}
									else
										updateChild(child);
							}
						}
						else
						{
							T* parent = gi->parent;
							if (parent)
							{
								int pos = findItem(parent);
								dcassert(pos != -1);
								if (children.empty())
								{
									children.push_back(parent);
									parent->hits = -1;
									gi->parent = nullptr;
									SetItemState(pos, INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
								}
								updateItem(pos);
							}
						}
						if (children.empty()) groups.erase(item->getGroupCond());
					}
				}
			}
			else if (item->getStateFlags() & STATE_FLAG_VISIBLE)
				deleteItem(item);

			if (removeFromMemory && ownsItemData)
				delete item;
			else
				item->groupInfo.reset();
			if (resortFlag)
				resort();
		}

		void deleteAllNoLock()
		{
			if (ownsItemData)
			{
				int count = GetItemCount();
				for (int i = 0; i < count; i++)
				{
					T* item = getItemData(i);
					if (!item->groupInfo) delete item;
				}
				for (auto i = groups.cbegin(); i != groups.cend(); ++i)
				{
					for (T* item : i->second->children)
						delete item;
					delete i->second->parent;
				}
			}
			DeleteAllItems();
			groups.clear();
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

				if (!isAscending() && abs(comp) != 2)
					comp = -comp;

				if (comp < 0)
					high = mid - 1;
				else
					low = mid + 1;
			}

			comp = compareItems(a, b, static_cast<uint8_t>(sortColumn));
			if (!isAscending() && abs(comp) != 2)
				comp = -comp;
			if (comp > 0)
				mid++;

			return mid;
		}
		GroupMap& getParents() { return groups; }

	protected:
		void sortItems() override
		{
			SortItems(&compareFunc, reinterpret_cast<LPARAM>(this));
		}

	private:
		GroupMap groups;
		CImageList states; // +/- images

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

			const T* aParent = nullptr;
			if (a->groupInfo && a->groupInfo->parent != a) aParent = a->groupInfo->parent;
			const T* bParent = nullptr;
			if (b->groupInfo && b->groupInfo->parent != b) bParent = b->groupInfo->parent;

			// both are children
			if (aParent && bParent)
			{
				// different parent
				if (aParent != bParent)
					return compareItems(aParent, bParent, col);
			}
			else
			{
				if (aParent == b)
					return 2;  // a should be displayed below b

				if (bParent == a)
					return -2; // b should be displayed below a

				if (aParent)
					return compareItems(aParent, b, col);

				if (bParent)
					return compareItems(a, bParent, col);
			}

			return T::compareItems(a, b, col);
		}

		int insertItemInternal(int pos, T* item, int image)
		{
			item->hits = -1;
			auto flags = item->getStateFlags() | STATE_FLAG_VISIBLE;
			if (image == I_IMAGECALLBACK)
				flags |= STATE_FLAG_USE_IMAGE_CALLBACK;
			else
				flags &= ~STATE_FLAG_USE_IMAGE_CALLBACK;
			item->setStateFlags(flags);
			return baseClass::insertItem(pos, item, image);
		}

		void insertChild(int pos, T* item)
		{
			insertItemInternal(pos, item,
				(item->getStateFlags() & STATE_FLAG_USE_IMAGE_CALLBACK) ? I_IMAGECALLBACK : item->getImageIndex());
		}

		void updateChild(T* item)
		{
			if (!(item->getStateFlags() & STATE_FLAG_USE_IMAGE_CALLBACK))
			{
				int pos = findItem(item);
				if (pos != -1) updateImage(pos, 0);
			}
		}
};

#endif // TYPED_TREE_LIST_VIEW_CTRL_H_

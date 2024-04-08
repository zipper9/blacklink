#ifndef TYPED_TREE_LIST_VIEW_CTRL_H_
#define TYPED_TREE_LIST_VIEW_CTRL_H_

#include "TypedListViewCtrl.h"
#include "LockRedraw.h"
#include "ResourceLoader.h"
#include "../client/BusyCounter.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4267)
#endif

// Copyright (C) 2005-2009 Big Muscle, StrongDC++
template<class T, class KValue, bool uniqueParent>
class TypedTreeListViewCtrl : public TypedListViewCtrl<T>
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

		typedef TypedTreeListViewCtrl<T, KValue, uniqueParent> thisClass;
		typedef TypedListViewCtrl<T> baseClass;

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
		MESSAGE_HANDLER(WM_KEYDOWN, onKeyDown)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP();

		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			ResourceLoader::LoadImageList(IDR_STATE, states, 16, 16);
			this->SetImageList(states, LVSIL_STATE);
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

			int pos = this->SubItemHitTest(&lvhti);
			if (pos != -1)
			{
				CRect rect;
				this->GetItemRect(pos, rect, LVIR_ICON);
				if (pt.x < rect.left)
				{
					T* i = this->getItemData(pos);
					if (i->groupInfo && i->groupInfo->parent == i)
					{
						if (i->getStateFlags() & STATE_FLAG_EXPANDED)
							collapse(i, pos);
						else
							expand(i, pos);
					}
				}
			}

			bHandled = FALSE;
			return 0;
		}

		LRESULT onKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			int mode = 0;
			if (GetKeyState(VK_CONTROL) >= 0)
				switch (wParam)
				{
					case VK_SUBTRACT:
						mode = -1;
						break;
					case VK_ADD:
						mode = 1;
						break;
				}
			if (!mode)
				return 0;
			int pos = this->GetNextItem(-1, LVNI_FOCUSED);
			if (pos < 0)
				return 0;
			auto ii = this->getItemData(pos);
			if (!ii || !ii->groupInfo || ii->groupInfo->parent != ii)
				return 0;
			if (mode == 1)
			{
				if (!(ii->getStateFlags() & STATE_FLAG_EXPANDED))
					expand(ii, pos);
			}
			else
			{
				if (ii->getStateFlags() & STATE_FLAG_EXPANDED)
					collapse(ii, pos);
			}
			bHandled = TRUE;
			return 0;
		}

		void collapse(T* parent, int itemPos)
		{
			this->SetRedraw(FALSE);
			if (parent->groupInfo)
			{
				const vector<T*>& children = parent->groupInfo->children;
				for (T* item : children)
				{
					if (this->deleteItem(item) != -1)
						item->setStateFlags(item->getStateFlags() & ~STATE_FLAG_VISIBLE);
				}
			}
			parent->setStateFlags(parent->getStateFlags() & ~STATE_FLAG_EXPANDED);
			this->SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			this->SetRedraw(TRUE);
		}

		void expand(T* parent, int itemPos)
		{
			this->SetRedraw(FALSE);
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
				this->SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
				this->resort();
			}
			this->SetRedraw(TRUE);
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
					this->deleteItem(parent);
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
					this->SetItemState(parentPos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
					if (resortFlag) this->resort();
				}
				else
				{
					parent->setStateFlags(parent->getStateFlags() & ~STATE_FLAG_EXPANDED);
					for (T* child : gi->children)
						if (child->getStateFlags() & STATE_FLAG_VISIBLE)
						{
							int pos = this->deleteItem(child);
							if (pos != -1 && pos < parentPos) parentPos--;
							child->setStateFlags(child->getStateFlags() & ~STATE_FLAG_VISIBLE);
						}
					this->SetItemState(parentPos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
				}
				return parentPos;
			}

			if (parent)
			{
				parent->hits++;
				if (parent->getStateFlags() & STATE_FLAG_EXPANDED)
					return insertItemInternal(getSortPos(item), item, useImageCallback ? I_IMAGECALLBACK : item->getImageIndex());
				item->setStateFlags(item->getStateFlags() & ~STATE_FLAG_VISIBLE);
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
							this->deleteItem(child);
							if (this->ownsItemData) delete child;
						}
					this->deleteItem(item);
					groups.erase(item->getGroupCond());
				}
				else
				{
					if (item->getStateFlags() & STATE_FLAG_VISIBLE) this->deleteItem(item);
					auto it = std::find(children.begin(), children.end(), item);
					if (it != children.end())
					{
						children.erase(it);
						if (uniqueParent && children.size() <= 1)
						{
							this->deleteItem(gi->parent);
							if (this->ownsItemData) delete gi->parent;
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
								int pos = this->findItem(parent);
								dcassert(pos != -1);
								if (children.empty())
								{
									children.push_back(parent);
									parent->hits = -1;
									gi->parent = nullptr;
									this->SetItemState(pos, INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
								}
								else
									parent->hits = children.size();
								this->updateItem(pos);
							}
						}
						if (children.empty()) groups.erase(item->getGroupCond());
					}
				}
			}
			else if (item->getStateFlags() & STATE_FLAG_VISIBLE)
				this->deleteItem(item);

			if (removeFromMemory && this->ownsItemData)
				delete item;
			else
				item->groupInfo.reset();
			if (resortFlag)
				this->resort();
		}

		void deleteAllNoLock()
		{
			if (this->ownsItemData)
			{
				int count = this->GetItemCount();
				for (int i = 0; i < count; i++)
				{
					T* item = this->getItemData(i);
					if (!item->groupInfo) delete item;
				}
				for (auto i = groups.cbegin(); i != groups.cend(); ++i)
				{
					for (T* item : i->second->children)
						delete item;
					delete i->second->parent;
				}
			}
			this->DeleteAllItems();
			groups.clear();
		}

		void deleteAll()
		{
			dcassert(!this->destroyingItems);
			BusyCounter<bool> busy(this->destroyingItems);
			CLockRedraw<> lockRedraw(this->m_hWnd);
			deleteAllNoLock();
		}

		int getSortPos(const T* a)
		{
			int high = this->GetItemCount();
			int sortColumn = this->getRealSortColumn();
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
				b = this->getItemData(mid);
				comp = compareItems(a, b, static_cast<uint8_t>(sortColumn));

				if (comp == 0)
					return mid;

				if (!this->isAscending() && abs(comp) != 2)
					comp = -comp;

				if (comp < 0)
					high = mid - 1;
				else
					low = mid + 1;
			}

			comp = compareItems(a, b, static_cast<uint8_t>(sortColumn));
			if (!this->isAscending() && abs(comp) != 2)
				comp = -comp;
			if (comp > 0)
				mid++;

			return mid;
		}
		GroupMap& getParents() { return groups; }

	protected:
		void sortItems() override
		{
			this->SortItems(&compareFunc, reinterpret_cast<LPARAM>(this));
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
				int pos = this->findItem(item);
				if (pos != -1) this->updateImage(pos, 0);
			}
		}
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // TYPED_TREE_LIST_VIEW_CTRL_H_

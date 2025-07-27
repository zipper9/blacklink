#ifndef CONTROL_LIST_H_
#define CONTROL_LIST_H_

#include "ControlTypes.h"
#include <vector>
#include "../client/w.h"

namespace WinUtil
{

class ControlList
{
	public:
		struct Item
		{
			HWND hWnd;
			int type;
			int id;
			int stringId;
		};

		void add(WinUtil::ControlType type, int id = -1, int stringId = -1);
		int find(HWND hWnd) const;
		void create(HWND hWndParent);
		void setCheck(int index, bool check);
		bool isChecked(int index) const;
		void show(int showCmd);
		const std::vector<Item>& getData() const { return data; }

	private:
		std::vector<Item> data;
};

}

#endif // CONTROL_LIST_H_

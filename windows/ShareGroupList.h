#ifndef SHARE_GROUP_LIST_H_
#define SHARE_GROUP_LIST_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlctrls.h>
#include "../client/CID.h"
#include "../client/Text.h"

class ShareGroupList
{
	public:
		static const int HIDE_SHARE_DEF = 1;

		struct Item
		{
			CID id;
			tstring name;
			int def;
		};

		vector<Item> v;

		void init();
		void fillCombo(CComboBox& ctrlShareGroup, const CID& shareGroup, bool hideShare);
};

#endif // SHARE_GROUP_LIST_H_

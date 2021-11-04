#include "stdafx.h"
#include "ShareGroupList.h"
#include "../client/ShareManager.h"

void ShareGroupList::init()
{
	vector<ShareManager::ShareGroupInfo> smGroups;
	ShareManager::getInstance()->getShareGroups(smGroups);
	v.clear();
	v.emplace_back(Item{CID(), TSTRING(SHARE_GROUP_DEFAULT), 0});
	v.emplace_back(Item{CID(), TSTRING(SHARE_GROUP_NOTHING), 1});
	for (const auto& sg : smGroups)
		v.emplace_back(Item{sg.id, Text::toT(sg.name), 2});
	sort(v.begin(), v.end(),
		[](const Item& a, const Item& b)
		{
			if (a.def != b.def) return a.def < b.def;
			return stricmp(a.name, b.name) < 0;
		});
}

void ShareGroupList::fillCombo(CComboBox& ctrlShareGroup, const CID& shareGroup, bool hideShare)
{
	int selIndex = -1;
	for (int i = 0; i < (int) v.size(); ++i)
	{
		const auto& sg = v[i];
		ctrlShareGroup.AddString(sg.name.c_str());
		if (selIndex < 0 && sg.def == 2 && shareGroup == sg.id)
			selIndex = i;
	}
	if (hideShare) selIndex = 1;
	if (selIndex < 0) selIndex = 0;
	ctrlShareGroup.SetCurSel(selIndex);
}

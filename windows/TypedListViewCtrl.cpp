#include "stdafx.h"
#include "TypedListViewCtrl.h"
#include "LockRedraw.h"
#include "../client/SimpleStringTokenizer.h"

static const int defaultWidth = 100;

void TypedListViewColumns::setColumns(int count, const int* ids, const ResourceManager::Strings* names, const int* widths)
{
	dcassert(columnList.empty());
	columnList.resize(count);
	for (int i = 0; i < count; ++i)
	{
		columnList[i].format = LVCFMT_LEFT;
		columnList[i].id = ids[i];
		columnList[i].name = TSTRING_I(names[i]);
		columnList[i].width = widths[i];
		columnList[i].isOwnerDraw = false;
		columnList[i].isVisible = false;
	}
}

void TypedListViewColumns::insertColumns(CListViewCtrl& lv, const string& order, const string& widths, const string& visible)
{
	struct TempColumnInfo
	{
		bool valid;
		bool visible;
		int width;
		int order;
		int pos;
	};

	int count = 0;
	for (const auto& c : columnList)
		if (c.id > count) count = c.id;
	++count;
	
	TempColumnInfo* info = static_cast<TempColumnInfo*>(_alloca(count * sizeof(TempColumnInfo)));
	for (int i = 0; i < count; ++i)
	{
		info[i].valid = false;
		info[i].visible = false;
		info[i].width = 0;
		info[i].order = -1;
		info[i].pos = -1;
	}
	for (size_t i = 0; i < columnList.size(); ++i)
	{
		int id = columnList[i].id;
		info[id].valid = true;
		info[id].visible = true;
		info[id].width = columnList[i].width;
		info[id].pos = i;
		columnList[i].isVisible = false;
	}
	
	SimpleStringTokenizer<char> st1(visible, ',');
	string tok;
	int index = 0;
	int visibleCount = 0;
	while (st1.getNextToken(tok))
	{
		if (index >= count) break;
		if (Util::toInt(tok))
			visibleCount++;
		else
			info[index].visible = false;
		++index;
	}

	if (!visibleCount)
	{
		for (int i = 0; i < count; ++i)
			info[i].visible = true;		
	}

	SimpleStringTokenizer<char> st2(widths, ',');
	index = 0;
	while (st2.getNextToken(tok))
	{
		if (index >= count) break;
		int width = Util::toInt(tok);
		if (width <= 0 || width > 2000) width = defaultWidth;
		info[index].width = width;
		++index;
	}

	int* pos = static_cast<int*>(_alloca(count * sizeof(int)));
	for (int i = 0; i < count; ++i)
		pos[i] = -1;

	SimpleStringTokenizer<char> st3(order, ',');
	index = 0;
	bool error = false;
	while (st3.getNextToken(tok))
	{
		if (index >= count) break;
		int ord = Util::toInt(tok);
		if (ord < 0 || ord >= count || pos[ord] != -1)
		{
			error = true;
			break;
		}
		pos[ord] = index;
		info[index].order = ord;
		++index;
	}

	if (error)
	{
		for (int i = 0; i < count; ++i)
			info[i].order = -1;
	}

	// Set order of items not found in settings
	for (int i = 0; i < count; ++i)
		if (info[i].valid && info[i].visible)
		{
			for (int j = 0; j < static_cast<int>(columnList.size()); ++j)
				if (columnList[j].id == i)
				{
					columnList[j].isVisible = true;
					if (info[i].order < 0)
					{
						int ord = 0;
						for (--j; j >= 0; --j)
						{
							int visibleOrder = info[columnList[j].id].order;
							if (visibleOrder >= 0)
							{
								ord = visibleOrder + 1;
								break;
							}
						}
						for (int k = 0; k < count; ++k)
							if (info[k].order >= ord)
								++info[k].order;
						info[i].order = ord;
					}
					break;
				}
		}

	// Hide invalid columns
	for (int i = 0; i < count; ++i)
		if (!(info[i].valid && info[i].visible) && info[i].order >= 0)
		{
			int ord = info[i].order;
			for (int k = 0; k < count; ++k)
				if (info[k].order >= ord)
					--info[k].order;
			info[i].order = -1;
		}

	columnToSubItem.resize(count, -1);

	int subItem = 0;
	LVCOLUMN lvc = {};
	lvc.mask = LVCF_TEXT | LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
	for (int i = 0; i < count; ++i)
	{
		if (info[i].pos < 0) continue;
		const auto& c = columnList[info[i].pos];
		if (c.isVisible)
		{
			lvc.pszText = const_cast<TCHAR*>(c.name.c_str());
			lvc.fmt = c.format;
			lvc.cx = info[c.id].width;
			lvc.iSubItem = c.id;
			lv.InsertColumn(subItem, &lvc);
			columnToSubItem[c.id] = subItem;
			subItemToColumn.push_back(c.id);
			pos[info[c.id].order] = subItem;
			++subItem;
		}
	}
	lv.SetColumnOrderArray(subItem, pos);
}

void TypedListViewColumns::insertDummyColumn(CListViewCtrl& lv)
{
	dcassert(columnList.empty());

	LVCOLUMN lvc = {};
	lvc.mask = LVCF_TEXT | LVCF_FMT | LVCF_WIDTH;
	lvc.pszText = const_cast<TCHAR*>(_T("Dummy"));
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = LVSCW_AUTOSIZE_USEHEADER;
	lv.InsertColumn(0, &lvc);

	columnList.push_back(ColumnInfo{ 0, _T("Dummy"), LVCFMT_LEFT, 100, true, false });
	subItemToColumn.push_back(0);
	columnToSubItem.push_back(0);
}

void TypedListViewColumns::saveSettings(const CListViewCtrl& lv, string& order, string& widths, string& visible) const
{
	struct TempColumnInfo
	{
		int order;
		int width;
	};

	int count = 0;
	for (const auto& c : columnList)
		if (c.id > count) count = c.id;
	++count;

	int headerItems = lv.GetHeader().GetItemCount();
	int* ord = static_cast<int*>(_alloca(headerItems * sizeof(int)));
	lv.GetColumnOrderArray(headerItems, ord);
	
	TempColumnInfo* info = static_cast<TempColumnInfo*>(_alloca(count * sizeof(TempColumnInfo)));
	for (int i = 0; i < count; ++i)
	{
		info[i].order = -1;
		info[i].width = defaultWidth;
	}
	for (const auto& c : columnList)
		info[c.id].width = c.width;

	LVCOLUMN lvc = {};
	lvc.mask = LVCF_WIDTH | LVCF_SUBITEM;
	for (int i = 0; i < headerItems; ++i)
	{
		lv.GetColumn(ord[i], &lvc);
		info[lvc.iSubItem].order = i;
		info[lvc.iSubItem].width = lvc.cx;
	}

	for (int i = 0; i < count; ++i)
	{
		if (!visible.empty()) visible += ',';
		if (info[i].order < 0)
		{
			visible += '0';
			info[i].order = headerItems++;
		}
		else
			visible += '1';

		if (!order.empty()) order += ',';
		order += Util::toString(info[i].order);

		if (!widths.empty()) widths += ',';
		widths += Util::toString(info[i].width);
	}
}

void TypedListViewColumns::showMenu(const POINT& pt, HWND hWnd)
{
	headerMenu.DestroyMenu();
	headerMenu.CreatePopupMenu();
	MENUINFO inf;
	inf.cbSize = sizeof(MENUINFO);
	inf.fMask = MIM_STYLE;
	inf.dwStyle = MNS_NOTIFYBYPOS;
	headerMenu.SetMenuInfo(&inf);

	for (size_t i = 0; i < columnList.size(); ++i)
	{
		headerMenu.AppendMenu(MF_STRING, IDC_HEADER_MENU, columnList[i].name.c_str());
		if (columnList[i].isVisible)
			headerMenu.CheckMenuItem(i, MF_BYPOSITION | MF_CHECKED);
		if (i == 0 && cantRemoveFirstColumn)
			headerMenu.EnableMenuItem(i, MF_BYPOSITION | MF_GRAYED | MF_DISABLED);
	}
	headerMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, hWnd);
}

void TypedListViewColumns::toggleColumn(CListViewCtrl& lv, int index, int& sortColumn, bool& doResort)
{
	doResort = false;
	int headerItems = lv.GetHeader().GetItemCount();
	ColumnInfo& c = columnList[index];
	c.isVisible = !c.isVisible;
	
	LVCOLUMN lvc = {};

	CLockRedraw<true> lockRedraw(lv);
	if (!c.isVisible)
	{
		lvc.mask = LVCF_SUBITEM;
		int pos = 0;
		for (int i = 0; i < headerItems; ++i)
		{
			lv.GetColumn(i, &lvc);
			if (lvc.iSubItem == c.id)
			{
				pos = i;
				break;
			}
		}
		lv.DeleteColumn(pos);
		subItemToColumn.erase(subItemToColumn.begin() + pos);
		columnToSubItem[c.id] = -1;
		if (sortColumn == pos)
		{
			sortColumn = -1;
			doResort = true;
		}
		else if (pos < sortColumn)
			--sortColumn;
	}
	else
	{
		int pos = 0;
		for (int j = index - 1; j >= 0; --j)
			if (columnList[j].isVisible)
			{
				lvc.mask = LVCF_SUBITEM | LVCF_ORDER;
				for (int i = 0; i < headerItems; ++i)
				{
					lv.GetColumn(i, &lvc);
					if (lvc.iSubItem == columnList[j].id)
					{
						pos = lvc.iOrder + 1;
						break;
					}
				}
				break;
			}

		lvc.mask = LVCF_TEXT | LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_ORDER;
		lvc.pszText = const_cast<TCHAR*>(c.name.c_str());
		lvc.fmt = c.format;
		lvc.cx = c.width;
		lvc.iSubItem = c.id;
		lvc.iOrder = pos;
		lv.InsertColumn(pos, &lvc);

		subItemToColumn.insert(subItemToColumn.begin() + pos, c.id);
		for (int& v : columnToSubItem)
			if (v >= 0 && v <= pos) ++v;
		columnToSubItem[c.id] = pos;
		c.isVisible = true;
		if (pos <= sortColumn)
			++sortColumn;
	}
}

void TypedListViewColumns::getInfoTip(CListViewCtrl& lv, NMLVGETINFOTIP* pInfoTip)
{
	if (!BOOLSETTING(SHOW_INFOTIPS)) return;
	const bool noColumnHeader = (lv.GetWindowLongPtr(GWL_STYLE) & LVS_NOCOLUMNHEADER) != 0;
	static const size_t BUF_SIZE = 300;
	TCHAR buf[BUF_SIZE];
	const int columnCount = lv.GetHeader().GetItemCount();
	std::vector<int> indices(columnCount);
	lv.GetColumnOrderArray(columnCount, indices.data());
	size_t outLen = 0;
	for (int i = 0; i < columnCount; ++i)
	{
		size_t prevLen = outLen;
		if (!noColumnHeader)
		{
			LVCOLUMN lvCol = {0};
			lvCol.mask = LVCF_TEXT;
			lvCol.pszText = buf;
			lvCol.cchTextMax = BUF_SIZE;
			if (lv.GetColumn(indices[i], &lvCol))
			{
				size_t len = _tcslen(lvCol.pszText);
				if (outLen + len + 2 >= INFOTIPSIZE) break; // no room
				memcpy(pInfoTip->pszText + outLen, lvCol.pszText, len*sizeof(TCHAR));
				outLen += len;
				_tcscpy(pInfoTip->pszText + outLen, _T(": "));
				outLen += 2;
			}
		}
		LVITEM lvItem = {0};
		lvItem.iItem = pInfoTip->iItem;
		int dataLen = lv.GetItemText(pInfoTip->iItem, indices[i], buf, BUF_SIZE);
		if (dataLen <= 0) // empty data, skip it
		{
			outLen = prevLen;
			continue;
		}
		if (outLen + dataLen + 2 >= INFOTIPSIZE) // no room, stop
		{
			outLen = prevLen;
			break;
		}
		memcpy(pInfoTip->pszText + outLen, buf, dataLen*sizeof(TCHAR));
		outLen += dataLen;
		_tcscpy(pInfoTip->pszText + outLen, _T("\r\n"));
		outLen += 2;
	}
			
	if (outLen > 2) outLen -= 2;
				
	pInfoTip->pszText[outLen] = 0;
	pInfoTip->cchTextMax = static_cast<int>(outLen);
}

#include "stdafx.h"
#include "ControlList.h"
#include "Fonts.h"
#include "../client/ResourceManager.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

using namespace WinUtil;

void ControlList::add(WinUtil::ControlType type, int id, int stringId)
{
	data.push_back(Item{ nullptr, type, id, stringId });
}

int ControlList::find(HWND hWnd) const
{
	for (size_t i = 0; i < data.size(); ++i)
		if (data[i].hWnd == hWnd) return (int) i;
	return -1;
}

void ControlList::create(HWND hWndParent)
{
	for (Item& item : data)
	{
		if (item.hWnd) continue;
		const TCHAR* winClass = nullptr;
		unsigned style = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE;
		switch (item.type)
		{
			case CTRL_TEXT:
				winClass = _T("static");
				break;
			case CTRL_BUTTON:
				winClass = _T("button");
				style |= WS_TABSTOP;
				break;
			case CTRL_CHECKBOX:
				winClass = _T("button");
				style |= WS_TABSTOP | BS_AUTOCHECKBOX;
				break;
			case CTRL_RADIO:
				winClass = _T("button");
				style |= WS_TABSTOP | BS_AUTORADIOBUTTON;
				break;
			case CTRL_EDIT:
				winClass = _T("edit");
				style |= WS_TABSTOP;
				break;
			default:
				dcassert(0);
		}
		if (!winClass) continue; // TODO: add more control types
		const TCHAR* text = item.stringId != -1 ?
			CTSTRING_I((ResourceManager::Strings) item.stringId) : nullptr;
		HWND hWnd = CreateWindowEx(0, winClass, text, style, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hWndParent,
			(HMENU)(INT_PTR) item.id, (HINSTANCE) &__ImageBase, nullptr);
		if (!hWnd) continue;
		SendMessage(hWnd, WM_SETFONT, (WPARAM) Fonts::g_systemFont, FALSE);
		item.hWnd = hWnd;
	}
}

void ControlList::setCheck(int index, bool check)
{
	dcassert(data[index].type == CTRL_CHECKBOX || data[index].type == CTRL_RADIO);
	if (data[index].hWnd)
		SendMessage(data[index].hWnd, BM_SETCHECK, check ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool ControlList::isChecked(int index) const
{
	dcassert(data[index].type == CTRL_CHECKBOX || data[index].type == CTRL_RADIO);
	if (!data[index].hWnd)
		return false;
	return SendMessage(data[index].hWnd, BM_GETCHECK, 0, 0) != BST_UNCHECKED;
}

void ControlList::show(int showCmd)
{
	for (Item& item : data)
		if (item.hWnd)
			ShowWindow(item.hWnd, showCmd);
}

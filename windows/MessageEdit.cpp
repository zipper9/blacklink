#include <stdafx.h>
#include "MessageEdit.h"
#include "WinUtil.h"
#include "ConfUI.h"
#include "../client/SettingsManager.h"

LRESULT MessageEdit::onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{				
	switch (wParam)
	{
		case 1: // Ctrl + A
			SetSelAll();
			break;

		case VK_RETURN:
		case '\n':
			if (!(callback && callback->processEnter()))
				bHandled = FALSE;
			break;

		default:
			bHandled = FALSE;
	}
	if (callback && !bHandled && wParam != VK_BACK)
		callback->typingNotification();
	return 0;
}

LRESULT MessageEdit::onKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == VK_TAB)
	{
		bool isShift = WinUtil::isShift();
		if (isShift || GetWindowTextLength() == 0)
		{
			dcassert(GetFocus() == m_hWnd);
			HWND hWndParent = GetParent();
			HWND hWndNext = ::GetNextDlgTabItem(hWndParent, m_hWnd, isShift);
			if (hWndNext) ::SetFocus(hWndNext);
		}
		else if (!(callback && callback->handleAutoComplete()))
			bHandled = FALSE;
		return 0;
	}

	if (callback)
		callback->clearAutoComplete();

	if (insertHistoryLine(wParam))
		return 0;

	if (callback && callback->handleKey(wParam))
		return 0;

	bHandled = FALSE;
	return 0;
}

bool MessageEdit::insertHistoryLine(UINT chr)
{
	bool allowToInsertLineHistory = WinUtil::isAlt() ||
		(WinUtil::isCtrl() && SettingsManager::instance.getUiSettings()->getBool(Conf::USE_CTRL_FOR_LINE_HISTORY));
	if (!allowToInsertLineHistory)
		return false;

	int len;
	switch (chr)
	{
		case VK_UP:
			//scroll up in chat command history
			//currently beyond the last command?
			if (curCommandPosition > 0)
			{
				//check whether current command needs to be saved
				if (curCommandPosition == prevCommands.size())
					WinUtil::getWindowText(m_hWnd, currentCommand);
					//replace current chat buffer with current command
				{
					SetWindowText(prevCommands[--curCommandPosition].c_str());
					if (callback)
						callback->updateEditHeight();
				}
			}
			// move cursor to end of line
			len = GetWindowTextLength();
			SetSel(len, len);
			return true;

		case VK_DOWN:
			//scroll down in chat command history
			//currently beyond the last command?
			if (curCommandPosition + 1 < prevCommands.size())
			{
				//replace current chat buffer with current command
				SetWindowText(prevCommands[++curCommandPosition].c_str());
			}
			else if (curCommandPosition + 1 == prevCommands.size())
			{
				//revert to last saved, unfinished command
				SetWindowText(currentCommand.c_str());
				++curCommandPosition;
			}
			// move cursor to end of line
			len = GetWindowTextLength();
			SetSel(len, len);
			if (callback)
				callback->updateEditHeight();
			return true;

		case VK_HOME:
			if (!prevCommands.empty())
			{
				curCommandPosition = 0;
				WinUtil::getWindowText(m_hWnd, currentCommand);
				SetWindowText(prevCommands[curCommandPosition].c_str());
				if (callback)
					callback->updateEditHeight();
			}
			return true;

		case VK_END:
			curCommandPosition = prevCommands.size();
			SetWindowText(currentCommand.c_str());
			if (callback)
				callback->updateEditHeight();
			return true;
	}
	return false;
}

void MessageEdit::saveCommand(const tstring& text)
{
	for (auto it = prevCommands.begin(); it != prevCommands.end(); ++it)
		if (*it == text)
		{
			prevCommands.erase(it);
			break;
		}
	prevCommands.push_back(text);
	curCommandPosition = prevCommands.size();
	currentCommand.clear();
}

/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdafx.h"
#include "MessagePanel.h"
#include "WinUtil.h"
#include "ConfUI.h"
#include "../client/ClientManager.h"
#include "../client/SettingsManager.h"
#include "../client/SysVersion.h"

#ifdef BL_UI_FEATURE_EMOTICONS
#include "../client/SimpleStringTokenizer.h"
#include "Emoticons.h"
#include "EmoticonsDlg.h"
#include "EmoticonPacksDlg.h"
#include "ChatTextParser.h"
#endif

static const int BUTTON_WIDTH = 26;
static const int BUTTON_HEIGHT = 26;
static const int EDIT_HEIGHT = 22;

MessagePanel::MessagePanel(CEdit& ctrlMessage)
	: m_hWnd(nullptr), ctrlMessage(ctrlMessage), initialized(false),
	showSelectHubButton(false), showCCPMButton(false), disableChat(false),
	ccpmState(CCPM_STATE_DISCONNECTED)
{
}

void MessagePanel::destroyPanel()
{
	for (int i = 0; i < MAX_BUTTONS; ++i)
		ctrlButtons[i].DestroyWindow();
	tooltip.DestroyWindow();
	initialized = false;
}

void MessagePanel::initPanel(HWND hWnd)
{
	initialized = true;
	m_hWnd = hWnd;
	RECT rcDefault = {};
	tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(tooltip.IsWindow());

	createButton(BUTTON_SEND, IconBitmaps::EDITOR_SEND, IDC_SEND_MESSAGE, ResourceManager::BBCODE_PANEL_SENDMESSAGE);
	createButton(BUTTON_MULTILINE, IconBitmaps::EDITOR_MULTILINE, IDC_MESSAGEPANEL, ResourceManager::BBCODE_PANEL_MESSAGEPANELSIZE);
#ifdef BL_UI_FEATURE_EMOTICONS
	createButton(BUTTON_EMOTICONS, IconBitmaps::EDITOR_EMOTICON, IDC_EMOT, ResourceManager::BBCODE_PANEL_EMOTICONS);
#endif
	createButton(BUTTON_TRANSCODE, IconBitmaps::EDITOR_TRANSCODE, IDC_TRANSCODE, ResourceManager::BBCODE_PANEL_TRANSLATE);
	createButton(BUTTON_BOLD, IconBitmaps::EDITOR_BOLD, IDC_BOLD, ResourceManager::BBCODE_PANEL_BOLD);
	createButton(BUTTON_ITALIC, IconBitmaps::EDITOR_ITALIC, IDC_ITALIC, ResourceManager::BBCODE_PANEL_ITALIC);
	createButton(BUTTON_UNDERLINE, IconBitmaps::EDITOR_UNDERLINE, IDC_UNDERLINE, ResourceManager::BBCODE_PANEL_UNDERLINE);
	createButton(BUTTON_STRIKETHROUGH, IconBitmaps::EDITOR_STRIKE, IDC_STRIKE, ResourceManager::BBCODE_PANEL_STRIKE);
	createButton(BUTTON_LINK, IconBitmaps::EDITOR_LINK, IDC_LINK, ResourceManager::BBCODE_PANEL_LINK);
	createButton(BUTTON_COLOR, IconBitmaps::EDITOR_COLOR, IDC_COLOR, ResourceManager::BBCODE_PANEL_COLOR);
	createButton(BUTTON_FIND, IconBitmaps::EDITOR_FIND, IDC_FIND, ResourceManager::BBCODE_PANEL_FIND);
	createButton(BUTTON_SELECT_HUB, IconBitmaps::HUB_ONLINE, IDC_SELECT_HUB, ResourceManager::SELECT_HUB);
	createButton(BUTTON_CCPM, IconBitmaps::PADLOCK_OPEN, IDC_CCPM, ResourceManager::CONNECT_CCPM);

	tooltip.SetMaxTipWidth(200);
	if (SettingsManager::instance.getUiSettings()->getBool(Conf::CHAT_PANEL_SHOW_INFOTIPS))
		tooltip.Activate(TRUE);
}

void MessagePanel::createButton(int index, int image, int idc, ResourceManager::Strings caption)
{
	RECT rc = {};
	ctrlButtons[index].Create(m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | BS_ICON | BS_CENTER, 0, idc);
#ifdef OSVER_WIN_XP
	if (!SysVersion::isOsVistaPlus())
		imageButtons[index].SubclassWindow(ctrlButtons[index]);
#endif
	ctrlButtons[index].SetIcon(g_iconBitmaps.getIcon(image, 0));
	WinUtil::addTool(tooltip, ctrlButtons[index], caption);
}

void MessagePanel::updateButton(HDWP dwp, bool show, int index, CRect& rc)
{
	if (show)
	{
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		DeferWindowPos(dwp, ctrlButtons[index], nullptr,
			rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER | SWP_SHOWWINDOW);
	}
	else
	{
		DeferWindowPos(dwp, ctrlButtons[index], nullptr,
			0, 0, 0, 0, SWP_NOZORDER | SWP_HIDEWINDOW);
	}
}

void MessagePanel::updatePanel(const CRect& rect)
{
	dcassert(!ClientManager::isBeforeShutdown());
	tooltip.Activate(FALSE);
	if (m_hWnd == NULL)
		return;

	CRect rc = rect;

	rc.right = rc.left + 2;
	rc.bottom = rc.top + BUTTON_HEIGHT;

	const auto* ss = SettingsManager::instance.getUiSettings();
	HDWP dwp = BeginDeferWindowPos(MAX_BUTTONS);
	if (disableChat)
	{
		for (int i = 0; i < MAX_BUTTONS; i++)
			DeferWindowPos(dwp, ctrlButtons[i], nullptr,
				0, 0, 0, 0, SWP_NOZORDER | SWP_HIDEWINDOW);
	}
	else
	{
		updateButton(dwp, ss->getBool(Conf::SHOW_SEND_MESSAGE_BUTTON), BUTTON_SEND, rc);
		updateButton(dwp, ss->getBool(Conf::SHOW_MULTI_CHAT_BTN), BUTTON_MULTILINE, rc);
#ifdef BL_UI_FEATURE_EMOTICONS
		updateButton(dwp, ss->getBool(Conf::SHOW_EMOTICONS_BTN), BUTTON_EMOTICONS, rc);
#endif
#ifdef BL_UI_FEATURE_BB_CODES
		if (ss->getBool(Conf::SHOW_BBCODE_PANEL))
		{
			updateButton(dwp, ss->getBool(Conf::SHOW_TRANSCODE_BTN), BUTTON_TRANSCODE, rc);
			for (int i = BUTTON_TRANSCODE + 1; i < BUTTON_LINK; ++i)
				updateButton(dwp, true, i, rc);
			updateButton(dwp, ss->getBool(Conf::SHOW_LINK_BTN), BUTTON_LINK, rc);
			updateButton(dwp, ss->getBool(Conf::FORMAT_BB_CODES_COLORS), BUTTON_COLOR, rc);
		}
		else
#endif
		{
			for (int i = BUTTON_TRANSCODE; i <= BUTTON_COLOR; ++i)
				updateButton(dwp, false, i, rc);
		}
		updateButton(dwp, ss->getBool(Conf::SHOW_FIND_BTN), BUTTON_FIND, rc);
		updateButton(dwp, showSelectHubButton, BUTTON_SELECT_HUB, rc);
		updateButton(dwp, showCCPMButton, BUTTON_CCPM, rc);
	}
	EndDeferWindowPos(dwp);
	if (ss->getBool(Conf::CHAT_PANEL_SHOW_INFOTIPS))
		tooltip.Activate(TRUE);
}

int MessagePanel::getPanelWidth() const
{
	int width = 4;
	if (disableChat) return width;
	int count = 0;
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::SHOW_MULTI_CHAT_BTN)) ++count;
#ifdef BL_UI_FEATURE_EMOTICONS
	if (ss->getBool(Conf::SHOW_EMOTICONS_BTN)) ++count;
#endif
	if (ss->getBool(Conf::SHOW_SEND_MESSAGE_BUTTON)) ++count;
#ifdef BL_UI_FEATURE_BB_CODES
	if (ss->getBool(Conf::SHOW_BBCODE_PANEL))
	{
		count += 4;
		if (ss->getBool(Conf::SHOW_TRANSCODE_BTN)) ++count;
		if (ss->getBool(Conf::FORMAT_BB_CODES_COLORS)) ++count;
		if (ss->getBool(Conf::SHOW_LINK_BTN)) ++count;
	}
#endif
	if (ss->getBool(Conf::SHOW_FIND_BTN)) ++count;
	if (showSelectHubButton) ++count;
	if (showCCPMButton) ++count;
	width += BUTTON_WIDTH * count;
	return width;
}

void MessagePanel::setCCPMState(int state)
{
	if (ccpmState == state) return;
	ccpmState = state;
	if (ctrlButtons[BUTTON_CCPM])
	{
		HICON icon = g_iconBitmaps.getIcon(state == CCPM_STATE_CONNECTED || state == CCPM_STATE_CONNECTING ? IconBitmaps::PADLOCK_CLOSED : IconBitmaps::PADLOCK_OPEN, 0);
		ctrlButtons[BUTTON_CCPM].SetIcon(icon);
		ResourceManager::Strings caption;
		switch (state)
		{
			case CCPM_STATE_CONNECTED:
				caption = ResourceManager::DISCONNECT_CCPM;
				break;
			case CCPM_STATE_CONNECTING:
				caption = ResourceManager::CCPM_IN_PROGRESS;
				break;
			default:
				caption = ResourceManager::CONNECT_CCPM;
		}
		WinUtil::addTool(tooltip, ctrlButtons[BUTTON_CCPM], caption);
		ctrlButtons[BUTTON_CCPM].EnableWindow(state != CCPM_STATE_CONNECTING);
	}
}

#ifdef BL_UI_FEATURE_EMOTICONS
void MessagePanel::pasteText(const tstring& text)
{
	int start, end;
	ctrlMessage.GetSel(start, end);
	tstring message;
	WinUtil::getWindowText(ctrlMessage, message);
	message.replace(start, end - start, text);
	ctrlMessage.SetWindowText(message.c_str());
	start += text.length();
	ctrlMessage.SetSel(start, start);
}

LRESULT MessagePanel::onPasteText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (lParam)
	{
		tstring* text = reinterpret_cast<tstring*>(lParam);
		pasteText(*text);
		delete text;
	}
	return 0;
}

LRESULT MessagePanel::onEmoticons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& bHandled)
{
	tooltip.Activate(FALSE);
	if (hWndCtl != ctrlButtons[BUTTON_EMOTICONS].m_hWnd)
	{
		bHandled = FALSE;
		return 0;
	}
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (ss->getString(Conf::EMOTICONS_FILE) == "Disabled")
	{
		MessageBox(m_hWnd, CTSTRING(EMOTICONS_DISABLED), getAppNameVerT().c_str(), MB_ICONINFORMATION | MB_OK);
		return 0;
	}
	EmoticonsDlg dlg;
	ctrlButtons[BUTTON_EMOTICONS].GetWindowRect(dlg.pos);
	dlg.hWndNotif = m_hWnd;
	dlg.DoModal(m_hWnd);
	if (dlg.isError)
	{
		MessageBox(m_hWnd, CTSTRING(EMOTICONS_NOT_AVAILABLE), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
		return 0;
	}
	if (!dlg.result.empty())
		pasteText(dlg.result);
	ctrlMessage.SetFocus();
	if (ss->getBool(Conf::CHAT_PANEL_SHOW_INFOTIPS))
	{
		if (tooltip.IsWindow())
			tooltip.Activate(TRUE);
	}
	return 0;
}
#endif // BL_UI_FEATURE_EMOTICONS

BOOL MessagePanel::onContextMenu(POINT& pt, WPARAM& wParam)
{
#ifdef BL_UI_FEATURE_EMOTICONS
	if (reinterpret_cast<HWND>(wParam) == ctrlButtons[BUTTON_EMOTICONS])
	{
		showEmoticonsConfig(pt, m_hWnd);
		return TRUE;
	}
#endif
	return FALSE;
}

#ifdef BL_UI_FEATURE_EMOTICONS
static string formatNameList(const StringList& sl, size_t first)
{
	string s;
	for (size_t i = first; i < sl.size(); ++i)
	{
		if (!s.empty()) s += ';';
		s += sl[i];
	}
	return s;
}

void MessagePanel::showEmoticonsConfig(const POINT& pt, HWND hWnd)
{
	EmoticonPacksDlg dlg;
	if (!dlg.loadAvailablePacks())
	{
		MessageBox(hWnd, CTSTRING(EMOTICONS_NOT_AVAILABLE), getAppNameVerT().c_str(), MB_ICONWARNING | MB_OK);
		return;
	}

	StringList names;
	auto ss = SettingsManager::instance.getUiSettings();
	const string& mainFile = ss->getString(Conf::EMOTICONS_FILE);
	bool enabled = true;
	if (mainFile == "Disabled")
		enabled = false;
	else if (!mainFile.empty())
		names.push_back(mainFile);
	SimpleStringTokenizer<char> st(ss->getString(Conf::ADDITIONAL_EMOTICONS), ';');
	string token;
	while (st.getNextNonEmptyToken(token))
		names.push_back(token);

	dlg.enabled = enabled;
	for (const string& name : names)
		dlg.items.push_back(EmoticonPacksDlg::Item{Text::toT(name), true});
	if (dlg.DoModal(hWnd) != IDOK) return;

	names.clear();
	for (const auto& item : dlg.items)
		if (item.enabled) names.push_back(Text::fromT(item.name));

	if (dlg.enabled && !names.empty())
	{
		emoticonPackList.setConfig(names);
		ss->setString(Conf::EMOTICONS_FILE, names[0]);
		string s = formatNameList(names, 1);
		if (s.empty()) s += ';'; // prevent resetting to default value
		ss->setString(Conf::ADDITIONAL_EMOTICONS, s);
	}
	else
	{
		ss->setString(Conf::EMOTICONS_FILE, "Disabled");
		ss->setString(Conf::ADDITIONAL_EMOTICONS, formatNameList(names, 0));
		names.clear();
		emoticonPackList.setConfig(names);
	}
	chatTextParser.initSearch();
}
#endif

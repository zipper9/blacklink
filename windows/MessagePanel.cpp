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
#include "Resource.h"
#include "../client/ClientManager.h"
#include "../client/CompatibilityManager.h"

#ifdef IRAINMAN_INCLUDE_SMILE
#include "AGEmotionSetup.h"
#include "EmoticonsDlg.h"
#include "WinUtil.h"
#endif

static const int BUTTON_WIDTH = 26;
static const int BUTTON_HEIGHT = 26;
static const int EDIT_HEIGHT = 22;

HIconWrapper MessagePanel::g_hSendMessageIco(IDR_SENDMESSAGES_ICON);
HIconWrapper MessagePanel::g_hMultiChatIco(IDR_MULTI_CHAT_ICON);
#ifdef IRAINMAN_INCLUDE_SMILE
HIconWrapper MessagePanel::g_hEmoticonIco(IDR_SMILE_ICON);
#endif
HIconWrapper MessagePanel::g_hBoldIco(IDR_BOLD_ICON);
HIconWrapper MessagePanel::g_hUndelineIco(IDR_UNDERLINE_ICON);
HIconWrapper MessagePanel::g_hStrikeIco(IDR_STRIKE_ICON);
HIconWrapper MessagePanel::g_hItalicIco(IDR_ITALIC_ICON);
HIconWrapper MessagePanel::g_hTransCodeIco(IDR_TRANSCODE_ICON);
#ifdef SCALOLAZ_BB_COLOR_BUTTON
HIconWrapper MessagePanel::g_hColorIco(IDR_COLOR_ICON);
#endif

#ifdef IRAINMAN_INCLUDE_SMILE
OMenu MessagePanel::g_emoMenu;
int MessagePanel::emoMenuItemCount = 0;
#endif

MessagePanel::MessagePanel(CEdit& ctrlMessage)
	: m_hWnd(nullptr), ctrlMessage(ctrlMessage)
{
}

void MessagePanel::DestroyPanel()
{
#ifdef IRAINMAN_INCLUDE_SMILE
	ctrlEmoticons.DestroyWindow();
#endif
	ctrlSendMessageBtn.DestroyWindow();
	ctrlMultiChatBtn.DestroyWindow();
	ctrlBoldBtn.DestroyWindow();
	ctrlUnderlineBtn.DestroyWindow();
	ctrlStrikeBtn.DestroyWindow();
	ctrlItalicBtn.DestroyWindow();
	ctrlTransCodeBtn.DestroyWindow();
#ifdef SCALOLAZ_BB_COLOR_BUTTON
	ctrlColorBtn.DestroyWindow();
#endif
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
	ctrlSizeSel.DestroyWindow();
#endif
	tooltip.DestroyWindow();
}

void MessagePanel::InitPanel(HWND& hWnd, RECT& rcDefault)
{
	m_hWnd = hWnd;
	tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(tooltip.IsWindow());
#ifdef IRAINMAN_INCLUDE_SMILE
	ctrlEmoticons.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_EMOT);
	ctrlEmoticons.SetIcon(g_hEmoticonIco);
	tooltip.AddTool(ctrlEmoticons, ResourceManager::BBCODE_PANEL_EMOTICONS);
#endif // IRAINMAN_INCLUDE_SMILE
	
	ctrlSendMessageBtn.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_SEND_MESSAGE);
	ctrlSendMessageBtn.SetIcon(g_hSendMessageIco);
	tooltip.AddTool(ctrlSendMessageBtn, ResourceManager::BBCODE_PANEL_SENDMESSAGE);
	
	ctrlMultiChatBtn.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_MESSAGEPANEL);
	ctrlMultiChatBtn.SetIcon(g_hMultiChatIco);
	tooltip.AddTool(ctrlMultiChatBtn, ResourceManager::BBCODE_PANEL_MESSAGEPANELSIZE);
	
	ctrlBoldBtn.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_BOLD);
	ctrlBoldBtn.SetIcon(g_hBoldIco);
	tooltip.AddTool(ctrlBoldBtn, ResourceManager::BBCODE_PANEL_BOLD);
	
	ctrlUnderlineBtn.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_UNDERLINE);
	ctrlUnderlineBtn.SetIcon(g_hUndelineIco);
	tooltip.AddTool(ctrlUnderlineBtn, ResourceManager::BBCODE_PANEL_UNDERLINE);
	
	ctrlStrikeBtn.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_STRIKE);
	ctrlStrikeBtn.SetIcon(g_hStrikeIco);
	tooltip.AddTool(ctrlStrikeBtn, ResourceManager::BBCODE_PANEL_STRIKE);
	
	ctrlItalicBtn.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_ITALIC);
	ctrlItalicBtn.SetIcon(g_hItalicIco);
	tooltip.AddTool(ctrlItalicBtn, ResourceManager::BBCODE_PANEL_ITALIC);
	
	ctrlTransCodeBtn.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, ID_TEXT_TRANSCODE);
	ctrlTransCodeBtn.SetIcon(g_hTransCodeIco);
	tooltip.AddTool(ctrlTransCodeBtn, ResourceManager::BBCODE_PANEL_TRANSLATE);
	
#ifdef SCALOLAZ_BB_COLOR_BUTTON
	ctrlColorBtn.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_COLOR);
	ctrlColorBtn.SetIcon(g_hColorIco);
	tooltip.AddTool(ctrlColorBtn, ResourceManager::BBCODE_PANEL_COLOR);
#endif
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
	ctrlSizeSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL |
	                   WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
	ctrlSizeSel.SetFont(Fonts::g_font);
	
	ctrlSizeSel.AddString(L"-2");
	ctrlSizeSel.AddString(L"-1");
	ctrlSizeSel.AddString(L"+1");
	ctrlSizeSel.AddString(L"+2");
	ctrlSizeSel.SetCurSel(2);
#endif
	
	tooltip.SetMaxTipWidth(200);
	if (BOOLSETTING(CHAT_PANEL_SHOW_INFOTIPS))
		tooltip.Activate(TRUE);
}

void MessagePanel::UpdatePanel(const CRect& rect)
{
	dcassert(!ClientManager::isBeforeShutdown());
	tooltip.Activate(FALSE);
	if (m_hWnd == NULL)
		return;
		
	CRect rc = rect;

	rc.right = rc.left + 2;
	rc.bottom = rc.top + BUTTON_HEIGHT;

	if (BOOLSETTING(SHOW_SEND_MESSAGE_BUTTON))
	{
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		ctrlSendMessageBtn.ShowWindow(SW_SHOW);
		ctrlSendMessageBtn.MoveWindow(rc);
	}
	else
	{
		ctrlSendMessageBtn.ShowWindow(SW_HIDE);
	}
	
	if (BOOLSETTING(SHOW_MULTI_CHAT_BTN))
	{
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		ctrlMultiChatBtn.ShowWindow(SW_SHOW);
		ctrlMultiChatBtn.MoveWindow(rc);
	}
	else
	{
		ctrlMultiChatBtn.ShowWindow(SW_HIDE);
	}
#ifdef IRAINMAN_INCLUDE_SMILE
	if (BOOLSETTING(SHOW_EMOTICONS_BTN))
	{
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		ctrlEmoticons.ShowWindow(SW_SHOW);
		ctrlEmoticons.MoveWindow(rc);
	}
	else
	{
		ctrlEmoticons.ShowWindow(SW_HIDE);
	}
#endif // IRAINMAN_INCLUDE_SMILE
	if (BOOLSETTING(SHOW_BBCODE_PANEL))
	{
		// Transcode
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		ctrlTransCodeBtn.ShowWindow(SW_SHOW);
		ctrlTransCodeBtn.MoveWindow(rc);
		// Bold
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		ctrlBoldBtn.ShowWindow(SW_SHOW);
		ctrlBoldBtn.MoveWindow(rc);
		// Italic
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		ctrlItalicBtn.ShowWindow(SW_SHOW);
		ctrlItalicBtn.MoveWindow(rc);
		// Underline
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		ctrlUnderlineBtn.ShowWindow(SW_SHOW);
		ctrlUnderlineBtn.MoveWindow(rc);
		// Strike
		rc.left = rc.right;
		rc.right += BUTTON_WIDTH;
		ctrlStrikeBtn.ShowWindow(SW_SHOW);
		ctrlStrikeBtn.MoveWindow(rc);
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		if (BOOLSETTING(FORMAT_BB_CODES_COLORS))
		{
			// Color
			rc.left = rc.right;
			rc.right += BUTTON_WIDTH;
			ctrlColorBtn.ShowWindow(SW_SHOW);
			ctrlColorBtn.MoveWindow(rc);
		}
		else
		{
			ctrlColorBtn.ShowWindow(SW_HIDE);
		}
#endif // SCALOLAZ_BB_COLOR_BUTTON
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
		// Size Selection
		//rc.left = rc.right + 1;
		//rc.right += 40;
		//ctrlSizeSel.ShowWindow(SW_SHOW);
		//ctrlSizeSel.MoveWindow(rc);
		ctrlSizeSel.ShowWindow(SW_HIDE);// [!] SSA - Will enable on implementation of size-BBCode
#endif // FLYLINKDC_USE_BB_SIZE_CODE
	}
	else
	{
		ctrlBoldBtn.ShowWindow(SW_HIDE);
		ctrlStrikeBtn.ShowWindow(SW_HIDE);
		ctrlUnderlineBtn.ShowWindow(SW_HIDE);
		ctrlItalicBtn.ShowWindow(SW_HIDE);
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
		ctrlSizeSel.ShowWindow(SW_HIDE);
#endif
		ctrlTransCodeBtn.ShowWindow(SW_HIDE);
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		ctrlColorBtn.ShowWindow(SW_HIDE);
#endif
	}
	if (BOOLSETTING(CHAT_PANEL_SHOW_INFOTIPS))
		tooltip.Activate(TRUE);
}

int MessagePanel::GetPanelWidth()
{
	int width = 4;
	width += BOOLSETTING(SHOW_MULTI_CHAT_BTN) ? BUTTON_WIDTH : 0;
#ifdef IRAINMAN_INCLUDE_SMILE
	width += BOOLSETTING(SHOW_EMOTICONS_BTN) ? BUTTON_WIDTH : 0;
#endif
	width += BOOLSETTING(SHOW_SEND_MESSAGE_BUTTON) ? BUTTON_WIDTH : 0;
	width += BOOLSETTING(SHOW_BBCODE_PANEL) ? BUTTON_WIDTH *
#ifdef SCALOLAZ_BB_COLOR_BUTTON
	                      6
#else   //SCALOLAZ_BB_COLOR_BUTTON
	                      5
#endif  //SCALOLAZ_BB_COLOR_BUTTON
	                      : 0;
	return width;
}

#ifdef IRAINMAN_INCLUDE_SMILE
LRESULT MessagePanel::onEmoticons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& bHandled)
{
	tooltip.Activate(FALSE);
	if (hWndCtl != ctrlEmoticons.m_hWnd)
	{
		bHandled = false;
		return 0;
	}
	EmoticonsDlg dlg;
	ctrlEmoticons.GetWindowRect(dlg.pos);
	dlg.DoModal(m_hWnd);
	if (!dlg.result.empty())
	{
		int start, end;
		ctrlMessage.GetSel(start, end);
		tstring message;
		WinUtil::getWindowText(ctrlMessage, message);
		message.replace(start, end - start, dlg.result);
		ctrlMessage.SetWindowText(message.c_str());
		ctrlMessage.SetFocus();
		start += dlg.result.length();
		ctrlMessage.SetSel(start, start);
	}
	if (BOOLSETTING(CHAT_PANEL_SHOW_INFOTIPS))
	{
		if (tooltip.IsWindow())
			tooltip.Activate(TRUE);
	}
	return 0;
}

LRESULT MessagePanel::onEmoPackChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	tooltip.Activate(FALSE);
	TCHAR buf[256];
	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_STRING;
	mii.cch = _countof(buf);
	mii.dwTypeData = buf;
	if (!g_emoMenu.GetMenuItemInfo(wID, FALSE, &mii))
		return -1;
	string emoticonsFile = Text::fromT(buf);
	if (SETTING(EMOTICONS_FILE) != emoticonsFile)
	{
		SET_SETTING(EMOTICONS_FILE, emoticonsFile);
		if (!CAGEmotionSetup::reCreateEmotionSetup())
			return -1;
	}
	if (/*!BOOLSETTING(POPUPS_DISABLED) && */BOOLSETTING(CHAT_PANEL_SHOW_INFOTIPS))
		tooltip.Activate(TRUE);
	return 0;
}
#endif // IRAINMAN_INCLUDE_SMILE

BOOL MessagePanel::OnContextMenu(POINT& pt, WPARAM& wParam)
{
#ifdef IRAINMAN_INCLUDE_SMILE
	if (reinterpret_cast<HWND>(wParam) == ctrlEmoticons)
	{
		showEmoticonsMenu(g_emoMenu, pt, m_hWnd, IDC_EMOMENU, emoMenuItemCount);
		return TRUE;
	}
#endif
	return FALSE;
}

#ifdef IRAINMAN_INCLUDE_SMILE
void MessagePanel::showEmoticonsMenu(OMenu& menu, const POINT& pt, HWND hWnd, int idc, int& count)
{
	count = 0;
	if (menu.m_hMenu)
		menu.DestroyMenu();
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, idc, CTSTRING(DISABLED));
	if (SETTING(EMOTICONS_FILE) == "Disabled")
		menu.CheckMenuItem(idc, MF_BYCOMMAND | MF_CHECKED);
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFileEx(Text::toT(Util::getEmoPacksPath() + "*.xml").c_str(),
	                               CompatibilityManager::findFileLevel,
	                               &data,
	                               FindExSearchNameMatch,
	                               nullptr,
	                               0);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			tstring name = data.cFileName;
			tstring::size_type i = name.rfind('.');
			name.erase(i);
			count++;
			menu.AppendMenu(MF_STRING, idc + count, name.c_str());
			if (name == Text::toT(SETTING(EMOTICONS_FILE)))
				menu.CheckMenuItem(idc + count, MF_BYCOMMAND | MF_CHECKED);
		}
		while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	if (count)
		menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, hWnd);
}
#endif

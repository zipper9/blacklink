/*
 * Copyright (C) 2012-2017 FlylinkDC++ Team http://flylinkdc.com
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
#include "BaseChatFrame.h"
#include "Commands.h"
#include "../client/QueueManager.h"

#if defined(IRAINMAN_USE_BB_CODES) && defined(SCALOLAZ_BB_COLOR_BUTTON)
static string RGB2HTMLHEX(int val)
{
	char buf[16];
	buf[0] = 0;
	_snprintf(buf, _countof(buf), "%.2X%.2X%.2X", GetRValue(val), GetGValue(val), GetBValue(val));
	return buf;
}
#endif

static void setBBCodeForCEdit(CEdit& ctrlMessage, WORD wID)
{
#ifdef IRAINMAN_USE_BB_CODES
#ifdef SCALOLAZ_BB_COLOR_BUTTON
	tstring startTag;
	tstring  endTag;
#else // SCALOLAZ_BB_COLOR_BUTTON
	TCHAR* startTag = nullptr;
	TCHAR* endTag = nullptr;
#endif // SCALOLAZ_BB_COLOR_BUTTON
	switch (wID)
	{
		case IDC_BOLD:
			startTag = _T("[b]");
			endTag = _T("[/b]");
			break;
		case IDC_ITALIC:
			startTag = _T("[i]");
			endTag = _T("[/i]");
			break;
		case IDC_UNDERLINE:
			startTag = _T("[u]");
			endTag = _T("[/u]");
			break;
		case IDC_STRIKE:
			startTag = _T("[s]");
			endTag = _T("[/s]");
			break;
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		case IDC_COLOR:
		{
			CColorDialog dlg(SETTING(TEXT_GENERAL_FORE_COLOR), 0, ctrlMessage.m_hWnd /*mainWnd*/);
			if (dlg.DoModal(ctrlMessage.m_hWnd) == IDOK)
			{
				const string hexString = RGB2HTMLHEX(dlg.GetColor());
				tstring tcolor = _T("[color=#") + (Text::toT(hexString)) + _T("]");
				startTag = tcolor;
				endTag = _T("[/color]");
			}
			break;
		}
#endif // SCALOLAZ_BB_COLOR_BUTTON
		
		//  case IDC_WIKI:
		//      startTag = _T("[ruwiki]");
		//      endTag = _T("[/ruwiki]");
		//      break;
	
		default:
			dcassert(0);
	}
	
	tstring setString;
	int nStartSel = 0;
	int nEndSel = 0;
	ctrlMessage.GetSel(nStartSel, nEndSel);
	tstring s;
	WinUtil::getWindowText(ctrlMessage, s);
	tstring startString = s.substr(0, nStartSel);
	tstring middleString = s.substr(nStartSel, nEndSel - nStartSel);
	tstring endString = s.substr(nEndSel, s.length() - nEndSel);
	setString = startString;
	
	if ((nEndSel - nStartSel) > 0) //„то-то выделено, обрамл€ем тэгом, курсор ставим в конце
	{
		setString += startTag;
		setString += middleString;
		setString += endTag;
		setString += endString;
		if (!setString.empty())
		{
			ctrlMessage.SetWindowText(setString.c_str());
			int newPosition = setString.length() - endString.length();
			ctrlMessage.SetSel(newPosition, newPosition);
		}
	}
	else    // Ќичего не выбрано, ставим тэги, курсор между ними
	{
		setString += startTag;
		const int newPosition = setString.length();
		setString += endTag;
		setString += endString;
		if (!setString.empty())
		{
			ctrlMessage.SetWindowText(setString.c_str());
			ctrlMessage.SetSel(newPosition, newPosition);
		}
	}
	ctrlMessage.SetFocus();
#endif
}

static TCHAR transcodeChar(const TCHAR msg)
{
	// TODO optimize this.
	static const TCHAR Lat[] = L"`qwertyuiop[]asdfghjkl;'zxcvbnm,./~!@#$%^&*()_+|QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>?";
	static const TCHAR Rus[] = L"Єйцукенгшщзхъфывапролджэ€чсмитьбю.®!\"є;%:?*()_+/…÷” ≈Ќ√Ўў«’Џ‘џ¬јѕ–ќЋƒ∆Ёя„—ћ»“№Ѕё,";
	for (size_t i = 0; i < _countof(Lat); i++)
	{
		if (msg == Lat[i])
			return Rus[i];
		else if (msg == Rus[i])
			return Lat[i];
	}
	return msg;
}

static void transcodeText(CEdit& ctrlMessage)
{
	const int len1 = static_cast<int>(ctrlMessage.GetWindowTextLength());
	if (len1 == 0)
		return;
	AutoArray<TCHAR> message(len1 + 1);
	ctrlMessage.GetWindowText(message, len1 + 1);
	
	int nStartSel = 0;
	int nEndSel = 0;
	ctrlMessage.GetSel(nStartSel, nEndSel);
	for (int i = 0; i < len1; i++)
	{
		if (nStartSel >= nEndSel || (i >= nStartSel && i < nEndSel))
		{
			message[i] = transcodeChar(message[i]);
		}
	}
	ctrlMessage.SetWindowText(message);
	//ctrlMessage.SetSel(ctrlMessage.GetWindowTextLength(), ctrlMessage.GetWindowTextLength());
	ctrlMessage.SetSel(nStartSel, nEndSel);
	ctrlMessage.SetFocus();
}

LRESULT BaseChatFrame::OnCreate(HWND p_hWnd, RECT &rcDefault)
{
	m_MessagePanelRECT = rcDefault;
	m_MessagePanelHWnd = p_hWnd;
	return 1;
}

void BaseChatFrame::destroyStatusbar(bool p_is_shutdown)
{
	if (ctrlStatus.m_hWnd)
	{
		HWND hwnd = ctrlStatus.Detach();
		::DestroyWindow(hwnd);
	}
	if (m_ctrlLastLinesToolTip.m_hWnd)
	{
		HWND hwnd = m_ctrlLastLinesToolTip.Detach();
		::DestroyWindow(hwnd);
	}
}

void BaseChatFrame::createStatusCtrl(HWND hWndStatusBar)
{
	ctrlStatus.Attach(hWndStatusBar);
	m_ctrlLastLinesToolTip.Create(ctrlStatus, m_MessagePanelRECT, _T("Fly_BaseChatFrame_ToolTips"), WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	m_ctrlLastLinesToolTip.SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	m_ctrlLastLinesToolTip.AddTool(ctrlStatus);
	m_ctrlLastLinesToolTip.SetDelayTime(TTDT_AUTOPOP, 15000);
}

void BaseChatFrame::destroyStatusCtrl()
{
}

void BaseChatFrame::createChatCtrl()
{
	if (!ctrlClient.IsWindow())
	{
		const auto l_res = ctrlClient.Create(m_MessagePanelHWnd, m_MessagePanelRECT, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                                     WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_READONLY, WS_EX_STATICEDGE, IDC_CLIENT);
		if (!l_res)
		{
			dcdebug("Error create BaseChatFrame::createChatCtrl %s", Util::translateError().c_str());
			dcassert(0);
		}
		else
		{
			ctrlClient.LimitText(0);
			ctrlClient.SetFont(Fonts::g_font);
			ctrlClient.SetAutoURLDetect(false);
			ctrlClient.SetEventMask(ctrlClient.GetEventMask() | ENM_LINK);
			ctrlClient.SetBackgroundColor(Colors::g_bgColor);
			if (m_is_suppress_chat_and_pm)
			{
				//   ctrlClient.ShowWindow(SW_HIDE);
			}
			else
			{
				readFrameLog();
			}
		}
	}
}

void BaseChatFrame::createMessageCtrl(ATL::CMessageMap *p_map, DWORD p_MsgMapID, bool p_is_suppress_chat_and_pm)
{
	m_is_suppress_chat_and_pm = p_is_suppress_chat_and_pm;
	dcassert(ctrlMessage == nullptr);
	createChatCtrl();
	ctrlMessage.Create(m_MessagePanelHWnd,
	                   m_MessagePanelRECT,
	                   NULL,
	                   WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
	                   WS_EX_CLIENTEDGE,
	                   IDC_CHAT_MESSAGE_EDIT);
	if (!m_LastMessage.empty())
	{
		ctrlMessage.SetWindowText(m_LastMessage.c_str());
		ctrlMessage.SetSel(m_LastSelPos);
	}
	ctrlMessage.SetFont(Fonts::g_font);
	ctrlMessage.SetLimitText(9999);
	m_ctrlMessageContainer = new CContainedWindow(WC_EDIT, p_map, p_MsgMapID);
	m_ctrlMessageContainer->SubclassWindow(ctrlMessage);
	
	if (p_is_suppress_chat_and_pm)
	{
		ctrlMessage.SetReadOnly();
		ctrlMessage.EnableWindow(FALSE);
	}
}

void BaseChatFrame::destroyMessageCtrl(bool p_is_shutdown)
{
	dcassert(!msgPanel); // ѕанелька должна быть разрушена до этого
	//safe_unsubclass_window(m_ctrlMessageContainer);
	if (ctrlMessage.m_hWnd)
	{
		if (!p_is_shutdown && ctrlMessage.IsWindow())
		{
			WinUtil::getWindowText(ctrlMessage, m_LastMessage);
			m_LastSelPos = ctrlMessage.GetSel();
		}
		HWND hwnd = ctrlMessage.Detach();
		::DestroyWindow(hwnd);
	}
	delete m_ctrlMessageContainer;
	m_ctrlMessageContainer = nullptr;
}

void BaseChatFrame::createMessagePanel()
{
	dcassert(!ClientManager::isBeforeShutdown());
	
	if (!msgPanel && ClientManager::isStartup() == false)
	{
		msgPanel = new MessagePanel(ctrlMessage);
		msgPanel->InitPanel(m_MessagePanelHWnd, m_MessagePanelRECT);
		ctrlClient.restore_chat_cache();
	}
}

void BaseChatFrame::destroyMessagePanel(bool p_is_shutdown)
{
	if (msgPanel)
	{
		msgPanel->DestroyPanel(p_is_shutdown);
		delete msgPanel;
		msgPanel = nullptr;
	}
}

void BaseChatFrame::setStatusText(unsigned char p_index, const tstring& p_text)
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(p_index < ctrlStatusCache.size());
	if (p_index < ctrlStatusCache.size())
	{
		ctrlStatusCache[p_index].second = ctrlStatusCache[p_index].first != p_text;
		if (ctrlStatusCache[p_index].second)
		{
			ctrlStatusCache[p_index].first = p_text;
			if (ctrlStatus)
			{
				ctrlStatus.SetText(p_index, p_text.c_str());
				ctrlStatusCache[p_index].second = false;
			}
		}
	}
}

void BaseChatFrame::restoreStatusFromCache()
{
	CLockRedraw<true> l_lock_draw(ctrlStatus.m_hWnd);
	for (size_t i = 0 ; i < ctrlStatusCache.size(); ++i)
	{
		ctrlStatus.SetText(i, ctrlStatusCache[i].first.c_str());
		ctrlStatusCache[i].second = false;
	}
}

void BaseChatFrame::checkMultiLine()
{
	if (ctrlMessage && ctrlMessage.GetWindowTextLength() > 0)
	{
		tstring fullMessageText;
		WinUtil::getWindowText(ctrlMessage, fullMessageText);
		const unsigned l_count_lines = std::count(fullMessageText.cbegin(), fullMessageText.cend(), L'\r');
		if (l_count_lines != m_MultiChatCountLines)
		{
			m_MultiChatCountLines = std::min(l_count_lines, unsigned(10));
			UpdateLayout();
		}
	}
}
bool BaseChatFrame::adjustChatInputSize(BOOL& bHandled)
{
	bool needsAdjust = WinUtil::isCtrlOrAlt();
	if (BOOLSETTING(MULTILINE_CHAT_INPUT) && BOOLSETTING(MULTILINE_CHAT_INPUT_BY_CTRL_ENTER))  // [+] SSA - Added Enter for MulticharInput
	{
		needsAdjust = !needsAdjust;
	}
	if (needsAdjust)
	{
		bHandled = FALSE;
		if (!BOOLSETTING(MULTILINE_CHAT_INPUT) && !m_bUseTempMultiChat)
		{
			m_bUseTempMultiChat = true;
			UpdateLayout();
		}
	}
	checkMultiLine();
	return needsAdjust;
}

TCHAR BaseChatFrame::getChatRefferingToNick()
{
#ifdef SCALOLAZ_CHAT_REFFERING_TO_NICK
	return BOOLSETTING(CHAT_REFFERING_TO_NICK) ? _T(',') : _T(':');
#else
	return _T(',');
#endif
}

void BaseChatFrame::clearMessageWindow()
{
	if (ctrlMessage)
		ctrlMessage.SetWindowText(_T(""));
	m_MultiChatCountLines = 0;
}

LRESULT BaseChatFrame::onSearchFileInInternet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!ChatCtrl::g_sSelectedText.empty())
	{
		searchFileInInternet(wID, ChatCtrl::g_sSelectedText);
	}
	return 0;
}

LRESULT BaseChatFrame::onTextStyleSelect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlMessage)
		setBBCodeForCEdit(ctrlMessage, wID);
	return 0;
}

LRESULT BaseChatFrame::OnTextTranscode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlMessage)
		transcodeText(ctrlMessage);
	return 0;
}

LRESULT BaseChatFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND hWnd = (HWND)lParam;
	const HDC hDC = (HDC)wParam;
	if (hWnd == ctrlClient.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	else if (m_is_suppress_chat_and_pm == false && ctrlMessage && hWnd == ctrlMessage.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	bHandled = FALSE;
	return FALSE;
}

void BaseChatFrame::insertLineHistoryToChatInput(const WPARAM wParam, BOOL& bHandled)
{
	const bool allowToInsertLineHistory = (WinUtil::isAlt() || (WinUtil::isCtrl() && BOOLSETTING(USE_CTRL_FOR_LINE_HISTORY)));
	if (allowToInsertLineHistory)
	{
		switch (wParam)
		{
			case VK_UP:
				//scroll up in chat command history
				//currently beyond the last command?
				if (ctrlMessage)
				{
					if (m_curCommandPosition > 0)
					{
						//check whether current command needs to be saved
						if (m_curCommandPosition == m_prevCommands.size())
							WinUtil::getWindowText(ctrlMessage, m_currentCommand);
						//replace current chat buffer with current command
						ctrlMessage.SetWindowText(m_prevCommands[--m_curCommandPosition].c_str());
					}
					// move cursor to end of line
					ctrlMessage.SetSel(ctrlMessage.GetWindowTextLength(), ctrlMessage.GetWindowTextLength());
				}
				break;
			case VK_DOWN:
				//scroll down in chat command history
				//currently beyond the last command?
				if (ctrlMessage)
				{
					if (m_curCommandPosition + 1 < m_prevCommands.size())
					{
						//replace current chat buffer with current command
						ctrlMessage.SetWindowText(m_prevCommands[++m_curCommandPosition].c_str());
					}
					else if (m_curCommandPosition + 1 == m_prevCommands.size())
					{
						//revert to last saved, unfinished command
						ctrlMessage.SetWindowText(m_currentCommand.c_str());
						++m_curCommandPosition;
					}
					// move cursor to end of line
					ctrlMessage.SetSel(ctrlMessage.GetWindowTextLength(), ctrlMessage.GetWindowTextLength());
				}
				break;
			case VK_HOME:
				if (ctrlMessage)
				{
					if (!m_prevCommands.empty())
					{
						m_curCommandPosition = 0;
						WinUtil::getWindowText(ctrlMessage, m_currentCommand);
						ctrlMessage.SetWindowText(m_prevCommands[m_curCommandPosition].c_str());
					}
				}
				break;
			case VK_END:
				m_curCommandPosition = m_prevCommands.size();
				if (ctrlMessage)
					ctrlMessage.SetWindowText(m_currentCommand.c_str());
				break;
		}
	}
	else bHandled = FALSE;
}

LRESULT BaseChatFrame::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	checkMultiLine();
	return 0;
}

bool BaseChatFrame::processingServices(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	const bool isService = uMsg != WM_KEYDOWN;
	if (isService)
	{
		switch (wParam)
		{
			case VK_RETURN:
				adjustChatInputSize(bHandled);
				break;
			case VK_TAB:
				bHandled = TRUE;
				break;
			case 0x0A:
				if (m_bProcessNextChar)
				{
					bHandled = TRUE;
					break;
				}
			default:
				bHandled = FALSE;
				break;
		}
		m_bProcessNextChar = false;
		if (uMsg == WM_CHAR && ctrlMessage && GetFocus() == ctrlMessage.m_hWnd && wParam != VK_RETURN && wParam != VK_TAB && wParam != VK_BACK)
		{
			PLAY_SOUND(SOUND_TYPING_NOTIFY);
		}
	}
	return isService;
}

void BaseChatFrame::processingHotKeys(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (wParam == VK_ESCAPE)
	{
		// Clear find text and give the focus back to the message box
		if (ctrlMessage)
			ctrlMessage.SetFocus();
		if (ctrlClient.IsWindow())
		{
			ctrlClient.SetSel(-1, -1);
			ctrlClient.SendMessage(EM_SCROLL, SB_BOTTOM, 0);
			ctrlClient.InvalidateRect(NULL);
		}
		m_currentNeedle.clear();
	}
	// Processing find.
	else if (((wParam == VK_F3 && WinUtil::isShift()) || (wParam == 'F' && WinUtil::isCtrl())) && !WinUtil::isAlt())
	{
		findText(findTextPopup());
	}
	else if (wParam == VK_F3)
	{
		findText(m_currentNeedle.empty() ? findTextPopup() : m_currentNeedle);
	}
	// ~Processing find.
	else
	{
		// don't handle these keys unless the user is entering a message
		if (ctrlMessage && GetFocus() == ctrlMessage.m_hWnd)
		{
			switch (wParam)
			{
				case 'A':
					if (WinUtil::isCtrl() && !WinUtil::isAlt() && !WinUtil::isShift())
						ctrlMessage.SetSelAll();
					break;
				case VK_RETURN:
				{
					if (!adjustChatInputSize(bHandled))
					{
						onEnter();
						if (BOOLSETTING(MULTILINE_CHAT_INPUT) && BOOLSETTING(MULTILINE_CHAT_INPUT_BY_CTRL_ENTER))  // [+] SSA - Added Enter for MulticharInput
						{
							m_bProcessNextChar = true;
						}
					}
				}
				break;
				case VK_UP:
				case VK_DOWN:
				case VK_HOME:
				case VK_END:
					insertLineHistoryToChatInput(wParam, bHandled);
					break;
				case VK_PRIOR: // page up
					if (ctrlClient.IsWindow())
						ctrlClient.SendMessage(WM_VSCROLL, SB_PAGEUP);
					break;
				case VK_NEXT: // page down
					if (ctrlClient.IsWindow())
						ctrlClient.SendMessage(WM_VSCROLL, SB_PAGEDOWN);
					break;
				default:
					bHandled = FALSE;
			}
			checkMultiLine();
		}
		else
		{
			bHandled = FALSE;
		}
	}
}

LRESULT BaseChatFrame::OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	LPMSG pMsg = (LPMSG)lParam;
	if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST && m_ctrlLastLinesToolTip)
		m_ctrlLastLinesToolTip.RelayEvent(pMsg);
	return 0;
}

void BaseChatFrame::onEnter()
{
	bool resetInputMessageText = true;
	dcassert(ctrlMessage);
	if (ctrlMessage && ctrlMessage.GetWindowTextLength() > 0)
	{
		tstring fullMessageText;
		WinUtil::getWindowText(ctrlMessage, fullMessageText);
		
		// save command in history, reset current buffer pointer to the newest command
		m_curCommandPosition = m_prevCommands.size();       //this places it one position beyond a legal subscript
		if (!m_curCommandPosition || (m_curCommandPosition > 0 && m_prevCommands[m_curCommandPosition - 1] != fullMessageText))
		{
			++m_curCommandPosition;
			m_prevCommands.push_back(fullMessageText);
		}
		m_currentCommand.clear();
		
		// Process special commands
		if (fullMessageText[0] == '/')
		{
			tstring cmd = fullMessageText;
			tstring param;
			tstring message;
			tstring localMessage;
			tstring status;
			if (Commands::processCommand(cmd, param, message, status, localMessage))
			{
				if (!message.empty())
					sendMessage(message);
				if (!status.empty())
					addStatus(status);
				if (!localMessage.empty())
					addLine(localMessage, 0, Colors::g_ChatTextSystem);
			}
			else if (stricmp(cmd.c_str(), _T("clear")) == 0 ||
			         stricmp(cmd.c_str(), _T("cls")) == 0 ||
			         stricmp(cmd.c_str(), _T("c")) == 0)
			{
				if (ctrlClient.IsWindow())
					ctrlClient.Clear();
			}			
			else if (stricmp(cmd.c_str(), _T("ts")) == 0)
			{
				m_bTimeStamps = !m_bTimeStamps;
				if (m_bTimeStamps)
					addStatus(TSTRING(TIMESTAMPS_ENABLED));
				else
					addStatus(TSTRING(TIMESTAMPS_DISABLED));
			}			
			else if (stricmp(cmd.c_str(), _T("find")) == 0)
			{
				if (param.empty())
					param = findTextPopup();					
				findText(param);
			}
			else if (stricmp(cmd.c_str(), _T("me")) == 0)
			{
				sendMessage(param, true);
			}
			else
			{
				processFrameCommand(fullMessageText, cmd, param, resetInputMessageText);
			}
		}
		else
		{
			processFrameMessage(fullMessageText, resetInputMessageText);
		}
		if (resetInputMessageText)
		{
			clearMessageWindow();
			UpdateLayout();
			return;
		}
	}
	else
	{
		MessageBeep(MB_ICONEXCLAMATION);
	}
	
	if (m_bUseTempMultiChat)
	{
		m_bUseTempMultiChat = false;
		UpdateLayout();
	}
}

LRESULT BaseChatFrame::onWinampSpam(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring cmd, param, message, status, localMessage;
	if (SETTING(MEDIA_PLAYER) == SettingsManager::WinAmp)
	{
		cmd = _T("/winamp");
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::WinMediaPlayer)
	{
		cmd = _T("/wmp");
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::iTunes)
	{
		cmd = _T("/itunes");
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::WinMediaPlayerClassic)
	{
		cmd = _T("/mpc");
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::JetAudio)
	{
		cmd = _T("/ja");
	}
	else
	{
		addStatus(TSTRING(NO_MEDIA_SPAM));
		return 0;
	}
	if (Commands::processCommand(cmd, param, message, status, localMessage))
	{
		if (!message.empty())
			sendMessage(message);
		if (!status.empty())
			addStatus(status);
	}
	return 0;
}

tstring BaseChatFrame::findTextPopup()
{
	LineDlg finddlg;
	finddlg.title = TSTRING(SEARCH);
	finddlg.description = TSTRING(SPECIFY_SEARCH_STRING);
	if (finddlg.DoModal() == IDOK)
	{
		return finddlg.line;
	}
	return Util::emptyStringT;
}

void BaseChatFrame::findText(const tstring& needle) noexcept
{
	dcassert(ctrlClient.IsWindow());
	if (ctrlClient.IsWindow())
	{
		int max = ctrlClient.GetWindowTextLength();
		// a new search? reset cursor to bottom
		if (needle != m_currentNeedle || m_currentNeedlePos == -1)
		{
			m_currentNeedle = needle;
			m_currentNeedlePos = max;
		}
		// set current selection
		FINDTEXT ft = {0};
		ft.chrg.cpMin = m_currentNeedlePos;
		ft.lpstrText = needle.c_str();
		// empty search? stop
		if (!needle.empty())
		{
			// find upwards
			m_currentNeedlePos = (int)ctrlClient.SendMessage(EM_FINDTEXT, 0, (LPARAM) & ft);
			// not found? try again on full range
			if (m_currentNeedlePos == -1 && ft.chrg.cpMin != max)  // no need to search full range twice
			{
				m_currentNeedlePos = max;
				ft.chrg.cpMin = m_currentNeedlePos;
				m_currentNeedlePos = (int)ctrlClient.SendMessage(EM_FINDTEXT, 0, (LPARAM) & ft);
			}
			// found? set selection
			if (m_currentNeedlePos != -1)
			{
				ft.chrg.cpMin = m_currentNeedlePos;
				ft.chrg.cpMax = m_currentNeedlePos + static_cast<LONG>(needle.length());
				ctrlClient.SetFocus();
				ctrlClient.SendMessage(EM_EXSETSEL, 0, (LPARAM)&ft);
			}
			else
			{
				addStatus(TSTRING(STRING_NOT_FOUND) + _T(' ') + needle);
				m_currentNeedle.clear();
			}
		}
	}
}

void BaseChatFrame::addStatus(const tstring& aLine, const bool bInChat /*= true*/, const bool bHistory /*= true*/, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextSystem*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
		return;
	tstring line = _T('[') + Text::toT(Util::getShortTimeString()) + _T("] ") + aLine;
	if (line.size() > 512)
	{
		line.resize(512);
	}
	setStatusText(0, line);
	
	if (bHistory)
	{
		m_lastLinesList.push_back(line);
		while (m_lastLinesList.size() > static_cast<size_t>(MAX_CLIENT_LINES))
			m_lastLinesList.pop_front();
	}
	
	if (bInChat && BOOLSETTING(STATUS_IN_CHAT))
	{
		addLine(_T("*** ") + aLine, 1, Colors::g_ChatTextServer);
	}
}
tstring BaseChatFrame::getIpCountry(const string& ip, bool ts, bool p_ipInChat, bool p_countryInChat, bool p_ISPInChat)
{
	tstring l_result;
	if (!ip.empty())
	{
		l_result = ts ? _T(" | ") : _T(" ");
		if (p_ipInChat)
		{
			l_result += Text::toT(ip);
		}
		if (p_countryInChat || p_ISPInChat)
		{
			const Util::CustomNetworkIndex& l_location = Util::getIpCountry(ip);
			if (p_countryInChat && !l_location.getCountry().empty())
			{
				l_result += (p_ipInChat ? _T(" | ") : _T("")) + l_location.getCountry();
			}
			else
				p_countryInChat = false;
				
			if (p_ISPInChat && !l_location.getDescription().empty())
			{
				l_result += ((p_countryInChat || p_ipInChat) ? _T(" | ") : _T("")) + l_location.getDescription();
			}
			else
				p_ISPInChat = false;
		}
		if (!p_countryInChat && !p_ISPInChat)
			l_result += _T(" ");  // Fix Right Click Menu on IP without space after IP
	}
	return l_result;
}

void BaseChatFrame::addLine(const tstring& aLine, unsigned p_max_smiles, CHARFORMAT2& cf /*= Colors::g_ChatTextGeneral */)
{
	//TODO - RoLex - chat- LogManager::message("addLine Hub:" + getHubHint() + " Message: [" + Text::fromT(aLine) + "]");
	
#ifdef _DEBUG
	if (aLine.find(_T("&#124")) != tstring::npos)
	{
		dcassert(0);
	}
#endif
	if (m_bTimeStamps)
	{
		const ChatCtrl::CFlyChatCache l_message(ClientManager::getFlylinkDCIdentity(), false, true, _T('[') + Text::toT(Util::getShortTimeString()) + _T("] "), aLine, cf, false);
		ctrlClient.AppendText(l_message, p_max_smiles, true);
	}
	else
	{
		const ChatCtrl::CFlyChatCache l_message(ClientManager::getFlylinkDCIdentity(), false, true, Util::emptyStringT, aLine, cf, false);
		ctrlClient.AppendText(l_message, p_max_smiles, true);
	}
}

void BaseChatFrame::addLine(const Identity& from, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, unsigned p_max_smiles, const CHARFORMAT2& cf, tstring& extra)
{
	if (ctrlClient.IsWindow())
	{
		ctrlClient.AdjustTextSize();
	}
	const bool ipInChat = BOOLSETTING(IP_IN_CHAT);
	const bool countryInChat = BOOLSETTING(COUNTRY_IN_CHAT);
	const bool ISPInChat = BOOLSETTING(ISP_IN_CHAT);
	if (ipInChat || countryInChat || ISPInChat)
	{
		if (from.isFantomIP() == false)
		{
			const string l_ip = from.getIpAsString();
			extra = getIpCountry(l_ip, m_bTimeStamps, ipInChat, countryInChat, ISPInChat);
		}
	}
	if (m_bTimeStamps)
	{
		const ChatCtrl::CFlyChatCache l_message(from, bMyMess, bThirdPerson, _T('[') + Text::toT(Util::getShortTimeString()) + extra + _T("] "), aLine, cf, true);
		ctrlClient.AppendText(l_message, p_max_smiles, true);
	}
	else
	{
		const ChatCtrl::CFlyChatCache l_message(from, bMyMess, bThirdPerson, !extra.empty() ? _T('[') + extra + _T("] ") : Util::emptyStringT, aLine, cf, true);
		ctrlClient.AppendText(l_message, p_max_smiles, true);
	}
}

LRESULT BaseChatFrame::onGetToolTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTTDISPINFO* nm = (NMTTDISPINFO*)pnmh;
	m_lastLines.clear();
	m_lastLines.shrink_to_fit();
	for (auto i = m_lastLinesList.cbegin(); i != m_lastLinesList.cend(); ++i)
	{
		m_lastLines += *i;
		m_lastLines += _T("\r\n");
	}
	if (m_lastLines.size() > 2)
	{
		m_lastLines.erase(m_lastLines.size() - 2);
	}
	nm->lpszText = const_cast<TCHAR*>(m_lastLines.c_str());
	
	return 0;
}

LRESULT BaseChatFrame::onMultilineChatInputButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SET_SETTING(MULTILINE_CHAT_INPUT, !BOOLSETTING(MULTILINE_CHAT_INPUT));
	m_bUseTempMultiChat = false;
	UpdateLayout();
	checkMultiLine();
	return 0;
}

void BaseChatFrame::appendChatCtrlItems(OMenu& p_menu, const Client* client)
{
	if (!ChatCtrl::g_sSelectedIP.empty())
	{
		p_menu.InsertSeparatorFirst(ChatCtrl::g_sSelectedIP);
#ifdef IRAINMAN_ENABLE_WHOIS
		WinUtil::AppendMenuOnWhoisIP(p_menu, ChatCtrl::g_sSelectedIP, false);
		p_menu.AppendMenu(MF_SEPARATOR);
#endif
		if (client) // add menus, necessary only for windows hub here.
		{
			if (client->isOp())
			{
				//p_menu.AppendMenu(MF_SEPARATOR);
				p_menu.AppendMenu(MF_STRING, IDC_BAN_IP, (_T("!banip ") + ChatCtrl::g_sSelectedIP).c_str());
				p_menu.SetMenuDefaultItem(IDC_BAN_IP);
				p_menu.AppendMenu(MF_STRING, IDC_UNBAN_IP, (_T("!unban ") + ChatCtrl::g_sSelectedIP).c_str());
				
				p_menu.AppendMenu(MF_SEPARATOR);
			}
		}
	}
	
	p_menu.AppendMenu(MF_STRING, ID_EDIT_COPY, CTSTRING(COPY));
	p_menu.AppendMenu(MF_STRING, IDC_COPY_ACTUAL_LINE,  CTSTRING(COPY_LINE));
	if (!ChatCtrl::g_sSelectedURL.empty())
	{
		p_menu.AppendMenu(MF_STRING, IDC_COPY_URL, Util::isMagnetLink(ChatCtrl::g_sSelectedURL) ? CTSTRING(COPY_MAGNET_LINK) : CTSTRING(COPY_URL));
		
#ifdef IRAINMAN_ENABLE_WHOIS
		if (!Util::isMagnetLink(ChatCtrl::g_sSelectedURL))
		{
			p_menu.AppendMenu(MF_SEPARATOR);
			p_menu.AppendMenu(MF_STRING, IDC_WHOIS_URL, (TSTRING(WHO_IS) + _T(" URL: Bgp.He")/* + ChatCtrl::g_sSelectedURL*/).c_str());
			
		}
#endif
	}
	
	
	if (!ChatCtrl::g_sSelectedText.empty())   // [+] SCALOlaz: add Search for Selected Text in Chat
	{
		p_menu.AppendMenu(MF_SEPARATOR);
		appendInternetSearchItems(p_menu); // [!] IRainman fix.
	}
	p_menu.AppendMenu(MF_SEPARATOR);
	
	
	p_menu.AppendMenu(MF_STRING, ID_EDIT_SELECT_ALL, CTSTRING(SELECT_ALL));
	p_menu.AppendMenu(MF_STRING, ID_EDIT_CLEAR_ALL, CTSTRING(CLEAR));
	p_menu.AppendMenu(MF_SEPARATOR);
	
	
	p_menu.AppendMenu(MF_STRING, IDC_AUTOSCROLL_CHAT, CTSTRING(ASCROLL_CHAT));
	if (ctrlClient.GetAutoScroll())
	{
		p_menu.CheckMenuItem(IDC_AUTOSCROLL_CHAT, MF_BYCOMMAND | MF_CHECKED);
	}
}

void BaseChatFrame::appendNickToChat(const tstring& nick)
{
	dcassert(ctrlMessage)
	if (ctrlMessage)
	{
		tstring sUser(nick);
		tstring sText;
		int iSelBegin, iSelEnd;
		ctrlMessage.GetSel(iSelBegin, iSelEnd);
		WinUtil::getWindowText(ctrlMessage, sText);
		
		if (iSelBegin == 0 && iSelEnd == 0)
		{
			sUser += getChatRefferingToNick();
			sUser += _T(' ');
			if (sText.empty())
			{
				ctrlMessage.SetWindowText(sUser.c_str());
				ctrlMessage.SetFocus();
				int selection = static_cast<int>(sUser.length());
				ctrlMessage.SetSel(selection, selection);
			}
			else
			{
				ctrlMessage.ReplaceSel(sUser.c_str());
				ctrlMessage.SetFocus();
			}
		}
		else
		{
			sUser += _T(',');
			sUser += _T(' ');
			ctrlMessage.ReplaceSel(sUser.c_str());
			ctrlMessage.SetFocus();
		}
	}
}

void BaseChatFrame::appendLogToChat(const string& path, const size_t linesCount)
{
	static const int64_t LOG_SIZE_TO_READ = 64 * 1024;
	string buf;
	try
	{
		File f(path, File::READ, File::OPEN);
		const int64_t size = f.getSize();
		if (size > LOG_SIZE_TO_READ)
		{
			f.setPos(size - LOG_SIZE_TO_READ);
		}
		buf = f.read(LOG_SIZE_TO_READ);
	}
	catch (const FileException&)
	{
		// LogManager::message("BaseChatFrame::appendLogToChat, Error load = " + path + " Error = " + e.getError());
	}
	const bool l_is_utf = buf.compare(0, 3, "\xef\xbb\xbf", 3) == 0;
	const StringTokenizer<string> l_lines(l_is_utf ? buf.substr(3) : buf, "\r\n");
	size_t i = l_lines.getTokens().size() > (linesCount + 1) ? l_lines.getTokens().size() - linesCount : 0;
	ChatCtrl::CFlyChatCache l_message(ClientManager::getFlylinkDCIdentity(), false, true, Util::emptyStringT, Util::emptyStringT, Colors::g_ChatTextLog, true, false);
	{
		for (; i < l_lines.getTokens().size(); ++i)
		{
			l_message.m_Msg = Text::toT(l_lines.getTokens()[i] + '\n');
			ctrlClient.AppendText(l_message, 1, false);
		}
	}
}
bool BaseChatFrame::isMultiChat(int& p_h, int & p_chat_columns) const
{
	const bool bUseMultiChat = BOOLSETTING(MULTILINE_CHAT_INPUT) || m_bUseTempMultiChat || /* Fonts::g_fontHeightPixl > 16 || */  m_MultiChatCountLines > 1;
	if (bUseMultiChat && m_MultiChatCountLines)
	{
		p_h = Fonts::g_fontHeightPixl * m_MultiChatCountLines;
	}
	else
	{
		p_h = Fonts::g_fontHeightPixl;
	}
	p_chat_columns = bUseMultiChat ? 2 : 1;
	return bUseMultiChat;
}

OMenu* BaseChatFrame::createUserMenu()
{
	if (!m_userMenu)
	{
		m_userMenu = new OMenu;
		m_userMenu->CreatePopupMenu();
	}
	return m_userMenu;
}

void BaseChatFrame::destroyUserMenu()
{
	safe_delete(m_userMenu);
}

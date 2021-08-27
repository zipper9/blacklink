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
#include "WinUtil.h"
#include "Fonts.h"
#include "Commands.h"
#include "LineDlg.h"
#include "../client/StringTokenizer.h"
#include <tom.h>
#include <comdef.h>

#ifdef IRAINMAN_USE_BB_CODES
static tstring printColor(COLORREF color)
{
	TCHAR buf[16];
	_sntprintf(buf, _countof(buf), _T("%06X"), GetRValue(color)<<16 | GetGValue(color)<<8 | GetBValue(color));
	return buf;
}

static UINT_PTR CALLBACK chooseColorHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_INITDIALOG)
	{
		const CHOOSECOLOR* cc = reinterpret_cast<CHOOSECOLOR*>(lParam);
		const POINT* pt = reinterpret_cast<POINT*>(cc->lCustData);
		CRect rc;
		GetWindowRect(hwnd, &rc);
		MoveWindow(hwnd, pt->x - rc.Width(), pt->y - rc.Height(), rc.Width(), rc.Height(), FALSE);
	}
	return 0;
}
#endif

void BaseChatFrame::insertBBCode(WORD wID, HWND hwndCtl)
{
#ifdef IRAINMAN_USE_BB_CODES
	tstring startTag;
	tstring  endTag;
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
		case IDC_COLOR:
		{
			CHOOSECOLOR cc = { sizeof(cc) };
			cc.hwndOwner = ctrlMessage.m_hWnd;
			cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK;
			cc.rgbResult = SETTING(TEXT_GENERAL_FORE_COLOR);
			cc.lpCustColors = CColorDialog::GetCustomColors();
			cc.lpfnHook = chooseColorHook;
			RECT rc;
			::GetWindowRect(hwndCtl, &rc);
			POINT pt;
			pt.x = rc.right;
			pt.y = rc.top;
			cc.lCustData = (LPARAM) &pt;
			if (ChooseColor(&cc))
			{
				startTag = _T("[color=#") + printColor(cc.rgbResult) + _T("]");
				endTag = _T("[/color]");
			}
			break;
		}
		default:
			dcassert(0);
	}
	
	if (startTag.empty()) return;
	
	int startSel = 0;
	int endSel = 0;
	ctrlMessage.GetSel(startSel, endSel);
	tstring s;
	WinUtil::getWindowText(ctrlMessage, s);
	tstring setString = s.substr(0, startSel);
	tstring middleString = s.substr(startSel, endSel - startSel);
	tstring endString = s.substr(endSel, s.length() - endSel);
	
	if (endSel > startSel) // Has selection
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
	else // No selection, just add the tags
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
	static const TCHAR Rus[] = L"∏ÈˆÛÍÂÌ„¯˘Áı˙Ù˚‚‡ÔÓÎ‰Ê˝ˇ˜ÒÏËÚ¸·˛.®!\"π;%:?*()_+/…÷” ≈Õ√ÿŸ«’⁄‘€¬¿œ–ŒÀƒ∆›ﬂ◊—Ã»“‹¡ﬁ,";
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
	tstring message;
	WinUtil::getWindowText(ctrlMessage, message);
	if (message.empty()) return;

	int startSel = 0;
	int endSel = 0;
	ctrlMessage.GetSel(startSel, endSel);
	for (tstring::size_type i = 0; i < message.length(); i++)
	{
		if (startSel >= endSel || ((int) i >= startSel && (int) i < endSel))
			message[i] = transcodeChar(message[i]);
	}
	ctrlMessage.SetWindowText(message.c_str());
	//ctrlMessage.SetSel(ctrlMessage.GetWindowTextLength(), ctrlMessage.GetWindowTextLength());
	ctrlMessage.SetSel(startSel, endSel);
	ctrlMessage.SetFocus();
}

LRESULT BaseChatFrame::onCreate(HWND hWnd, RECT &rc)
{
	messagePanelRect = rc;
	messagePanelHwnd = hWnd;
	return 1;
}

void BaseChatFrame::destroyStatusbar()
{
	if (ctrlStatus.m_hWnd)
		ctrlStatus.DestroyWindow();
	if (ctrlLastLinesToolTip.m_hWnd)
		ctrlLastLinesToolTip.DestroyWindow();
}

void BaseChatFrame::createStatusCtrl(HWND hWnd)
{
	ctrlStatus.Attach(hWnd);
	ctrlLastLinesToolTip.Create(ctrlStatus, messagePanelRect, _T("Fly_BaseChatFrame_ToolTips"), WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	ctrlLastLinesToolTip.SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	ctrlLastLinesToolTip.AddTool(ctrlStatus);
	ctrlLastLinesToolTip.SetDelayTime(TTDT_AUTOPOP, 15000);
}

void BaseChatFrame::destroyStatusCtrl()
{
}

void BaseChatFrame::createChatCtrl()
{
	if (!ctrlClient.IsWindow())
	{
		HWND hwnd = ctrlClient.Create(messagePanelHwnd, messagePanelRect, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                              WS_TABSTOP | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_SAVESEL | ES_READONLY | ES_NOOLEDRAGDROP, WS_EX_STATICEDGE, IDC_CLIENT);
		if (!hwnd)
		{
			dcdebug("Error create BaseChatFrame::createChatCtrl %s", Util::translateError().c_str());
			dcassert(0);
		}
		else
		{
			ctrlClient.LimitText(0);
			ctrlClient.SetFont(Fonts::g_font);
			ctrlClient.SetAutoURLDetect(FALSE);
			ctrlClient.SetEventMask(ctrlClient.GetEventMask() | ENM_LINK);
			ctrlClient.SetBackgroundColor(Colors::g_bgColor);
			if (suppressChat)
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

void BaseChatFrame::createMessageCtrl(ATL::CMessageMap* messageMap, DWORD messageMapID, bool suppressChat)
{
	this->suppressChat = suppressChat;
	dcassert(ctrlMessage == nullptr);
	createChatCtrl();
	ctrlMessage.setCallback(this);
	ctrlMessage.Create(messagePanelHwnd,
	                   messagePanelRect,
	                   NULL,
	                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
	                   WS_EX_CLIENTEDGE,
	                   IDC_CHAT_MESSAGE_EDIT);
	if (!lastMessage.empty())
	{
		ctrlMessage.SetWindowText(lastMessage.c_str());
		ctrlMessage.SetSel(lastMessageSelPos);
	}
	ctrlMessage.SetFont(Fonts::g_font);
	ctrlMessage.SetLimitText(9999);
	//ctrlMessage.SetWindowPos(ctrlClient, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // Set Tab order
	if (suppressChat)
	{
		ctrlMessage.SetReadOnly();
		ctrlMessage.EnableWindow(FALSE);
	}
}

void BaseChatFrame::destroyMessageCtrl(bool isShutdown)
{
	dcassert(!msgPanel); // destroyMessagePanel must have been called before
	if (ctrlMessage.m_hWnd)
	{
		if (!isShutdown && ctrlMessage.IsWindow())
		{
			WinUtil::getWindowText(ctrlMessage, lastMessage);
			lastMessageSelPos = ctrlMessage.GetSel();
		}
		ctrlMessage.DestroyWindow();
	}
}

void BaseChatFrame::createMessagePanel()
{
	dcassert(!ClientManager::isBeforeShutdown());
	
	if (!msgPanel && !ClientManager::isStartup())
	{
		msgPanel = new MessagePanel(ctrlMessage);
		msgPanel->initPanel(messagePanelHwnd);
		ctrlClient.restoreChatCache();
	}
}

void BaseChatFrame::destroyMessagePanel()
{
	if (msgPanel)
	{
		msgPanel->destroyPanel();
		delete msgPanel;
		msgPanel = nullptr;
	}
}

void BaseChatFrame::setStatusText(unsigned char index, const tstring& text)
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(index < ctrlStatusCache.size());
	if (index >= ctrlStatusCache.size()) return;
	if (!ctrlStatus || ctrlStatusCache[index] != text)
	{
		ctrlStatusCache[index] = text;
		if (ctrlStatus) ctrlStatus.SetText(index, text.c_str(), SBT_NOTABPARSING);
	}
}

void BaseChatFrame::restoreStatusFromCache()
{
	CLockRedraw<true> lockRedraw(ctrlStatus.m_hWnd);
	for (size_t i = 0 ; i < ctrlStatusCache.size(); ++i)
		ctrlStatus.SetText(i, ctrlStatusCache[i].c_str(), SBT_NOTABPARSING);
}

void BaseChatFrame::checkMultiLine()
{
	if (ctrlMessage)
	{
		tstring fullMessageText;
		WinUtil::getWindowText(ctrlMessage, fullMessageText);
		const unsigned lines = std::count(fullMessageText.cbegin(), fullMessageText.cend(), _T('\r'));
		if (lines != multiChatLines)
		{
			multiChatLines = std::min(lines, 10u);
			UpdateLayout();
		}
	}
}

TCHAR BaseChatFrame::getChatRefferingToNick()
{
#ifdef SCALOLAZ_CHAT_REFFERING_TO_NICK
	return BOOLSETTING(CHAT_REFFERING_TO_NICK) ? _T(',') : _T(':');
#else
	return _T(',');
#endif
}

void BaseChatFrame::clearInputBox()
{
	if (ctrlMessage)
		ctrlMessage.SetWindowText(_T(""));
	multiChatLines = 0;
}

LRESULT BaseChatFrame::onSearchFileOnInternet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!ChatCtrl::g_sSelectedText.empty())
	{
		searchFileOnInternet(wID, ChatCtrl::g_sSelectedText);
	}
	return 0;
}

LRESULT BaseChatFrame::onSaveToFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	IRichEditOle *pOle = ctrlClient.getRichEditOle();
	if (!pOle) return 0;
	ITextDocument *pDoc = nullptr;
	pOle->QueryInterface(IID_ITextDocument, (void**) &pDoc);
	if (pDoc)
	{
		static const WinUtil::FileMaskItem types[] =
		{
			{ ResourceManager::FILEMASK_TEXT, _T("*.txt") },
			{ ResourceManager::FILEMASK_RTF,  _T("*.rtf") },
			{ ResourceManager::FILEMASK_ALL,  _T("*.*")   },
			{ ResourceManager::Strings(),     nullptr     }
		};
		tstring target;
		WinUtil::browseFile(target, ctrlClient, true, Util::emptyStringT, WinUtil::getFileMaskString(types).c_str(), _T("txt"));
		if (!target.empty())
		{
			long flags = tomCreateAlways;
			if (Util::getFileExt(target) == _T(".rtf"))
				flags |= tomRTF;
			else
				flags |= tomText;
			VARIANT var;
			VariantInit(&var);
			var.bstrVal = SysAllocString(target.c_str());
			var.vt = VT_BSTR;
			HRESULT hr = pDoc->Save(&var, flags, 1200); // NOTE: 1208 doesn't work
			SysFreeString(var.bstrVal);
			if (FAILED(hr))
			{
				tstring errorMsg = TSTRING(ERROR_SAVING_DOCUMENT);
				_com_error ce(hr);
				errorMsg += ce.ErrorMessage();
				MessageBox(ctrlClient, errorMsg.c_str(), getAppNameVerT().c_str(), MB_ICONERROR | MB_OK);
			}
		}
		pDoc->Release();
	}
	return 0;
}

LRESULT BaseChatFrame::onTextStyleSelect(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	if (ctrlMessage)
		insertBBCode(wID, hWndCtl);
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
	else if (!suppressChat && ctrlMessage && hWnd == ctrlMessage.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT BaseChatFrame::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	checkMultiLine();
	return 0;
}

LRESULT BaseChatFrame::onForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	LPMSG pMsg = (LPMSG)lParam;
	if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST && ctrlLastLinesToolTip)
		ctrlLastLinesToolTip.RelayEvent(pMsg);
	return 0;
}

void BaseChatFrame::onEnter()
{
	bool updateLayout = false;
	bool resetInputMessageText = true;
	dcassert(ctrlMessage);
	if (ctrlMessage && ctrlMessage.GetWindowTextLength() > 0)
	{
		tstring fullMessageText;
		WinUtil::getWindowText(ctrlMessage, fullMessageText);
		ctrlMessage.saveCommand(fullMessageText);
		
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
				showTimestamps = !showTimestamps;
				if (showTimestamps)
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
			clearInputBox();
			updateLayout = true;
		}
	}
	else
		MessageBeep(MB_ICONEXCLAMATION);

	if (updateLayout)
		UpdateLayout();
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
	LineDlg dlg;
	dlg.title = TSTRING(SEARCH);
	dlg.description = TSTRING(SPECIFY_SEARCH_STRING);
	dlg.icon = IconBitmaps::SEARCH;
	dlg.allowEmpty = false;
	if (dlg.DoModal() == IDOK)
		return dlg.line;
	return Util::emptyStringT;
}

void BaseChatFrame::findText(const tstring& needle) noexcept
{
	dcassert(ctrlClient.IsWindow());
	if (ctrlClient.IsWindow())
	{
		int max = ctrlClient.GetWindowTextLength();
		// a new search? reset cursor to bottom
		if (needle != currentNeedle || currentNeedlePos == -1)
		{
			currentNeedle = needle;
			currentNeedlePos = max;
		}
		// set current selection
		FINDTEXT ft = {0};
		ft.chrg.cpMin = currentNeedlePos;
		ft.lpstrText = needle.c_str();
		// empty search? stop
		if (!needle.empty())
		{
			// find upwards
			currentNeedlePos = (int)ctrlClient.SendMessage(EM_FINDTEXT, 0, (LPARAM) & ft);
			// not found? try again on full range
			if (currentNeedlePos == -1 && ft.chrg.cpMin != max)  // no need to search full range twice
			{
				currentNeedlePos = max;
				ft.chrg.cpMin = currentNeedlePos;
				currentNeedlePos = (LONG) ctrlClient.SendMessage(EM_FINDTEXT, 0, (LPARAM) & ft);
			}
			// found? set selection
			if (currentNeedlePos != -1)
			{
				ft.chrg.cpMin = currentNeedlePos;
				ft.chrg.cpMax = currentNeedlePos + static_cast<LONG>(needle.length());
				ctrlClient.SetFocus();
				ctrlClient.SendMessage(EM_EXSETSEL, 0, (LPARAM)&ft);
			}
			else
			{
				addStatus(TSTRING(STRING_NOT_FOUND) + _T(' ') + needle);
				currentNeedle.clear();
			}
		}
	}
}

void BaseChatFrame::addStatus(const tstring& line, const bool inChat /*= true*/, const bool history /*= true*/, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextSystem*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
		return;
	tstring formattedLine = _T('[') + Text::toT(Util::getShortTimeString()) + _T("] ") + line;
	if (formattedLine.length() > 512)
		formattedLine.resize(512);
	setStatusText(0, formattedLine);
	
	if (history)
	{
		lastLinesList.push_back(formattedLine);
		while (lastLinesList.size() > static_cast<size_t>(MAX_CLIENT_LINES))
			lastLinesList.pop_front();
	}
	
	if (inChat && BOOLSETTING(STATUS_IN_CHAT))
		addSystemMessage(line, Colors::g_ChatTextServer);
}

void BaseChatFrame::addSystemMessage(const tstring& line, CHARFORMAT2& cf)
{
	addLine(_T("*** ") + line, 1, cf);
}

tstring BaseChatFrame::getIpCountry(const IpAddress& ip, bool ts, bool ipInChat, bool countryInChat, bool locationInChat)
{
	tstring result;
	if (Util::isValidIp(ip))
	{
		result = ts ? _T(" | ") : _T(" ");
		if (ipInChat)
			result += Util::printIpAddressT(ip);
		if ((countryInChat || locationInChat) && ip.type == AF_INET)
		{
			IPInfo ipInfo;
			Util::getIpInfo(ip.data.v4, ipInfo, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
			if (countryInChat)
			{
				if (!ipInfo.country.empty())
				{
					if (ipInChat) result += _T(" | ");
					result += Text::toT(ipInfo.country);
				}
				else
					countryInChat = false;
			}				
			if (locationInChat)
			{
				if (!ipInfo.location.empty())
				{
					if (ipInChat || countryInChat) result += _T(" | ");
					result += Text::toT(ipInfo.location);
				}
				else
					locationInChat = false;
			}
		}
		if (!countryInChat && !locationInChat)
			result += _T(" ");  // Fix Right Click Menu on IP without space after IP
	}
	return result;
}

void BaseChatFrame::addLine(const tstring& line, unsigned maxSmiles, CHARFORMAT2& cf /*= Colors::g_ChatTextGeneral */)
{
#ifdef _DEBUG
	if (line.find(_T("&#124")) != tstring::npos)
	{
		dcassert(0);
	}
#endif
	if (showTimestamps)
	{
		const ChatCtrl::Message message(nullptr, false, true, _T('[') + Text::toT(Util::getShortTimeString()) + _T("] "), line, cf, false);
		ctrlClient.appendText(message, maxSmiles);
	}
	else
	{
		const ChatCtrl::Message message(nullptr, false, true, Util::emptyStringT, line, cf, false);
		ctrlClient.appendText(message, maxSmiles);
	}
}

void BaseChatFrame::addLine(const Identity& from, const bool myMessage, const bool thirdPerson, const tstring& line, unsigned maxSmiles, const CHARFORMAT2& cf, tstring& extra)
{
	if (ctrlClient.IsWindow())
	{
		ctrlClient.adjustTextSize();
	}
	const bool ipInChat = BOOLSETTING(IP_IN_CHAT);
	const bool countryInChat = BOOLSETTING(COUNTRY_IN_CHAT);
	const bool ISPInChat = BOOLSETTING(ISP_IN_CHAT);
	if (ipInChat || countryInChat || ISPInChat)
	{
		IpAddress ip = from.getConnectIP();
		if (ip.type && !from.isIPCached(ip.type))
			extra = getIpCountry(ip, showTimestamps, ipInChat, countryInChat, ISPInChat);
	}
	if (showTimestamps)
	{
		const ChatCtrl::Message message(&from, myMessage, thirdPerson, _T('[') + Text::toT(Util::getShortTimeString()) + extra + _T("] "), line, cf, true);
		ctrlClient.appendText(message, maxSmiles);
	}
	else
	{
		const ChatCtrl::Message message(&from, myMessage, thirdPerson, !extra.empty() ? _T('[') + extra + _T("] ") : Util::emptyStringT, line, cf, true);
		ctrlClient.appendText(message, maxSmiles);
	}
}

LRESULT BaseChatFrame::onGetToolTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTTDISPINFO* nm = (NMTTDISPINFO*)pnmh;
	lastLines.clear();
	bool appendNL = false;
	for (auto i = lastLinesList.cbegin(); i != lastLinesList.cend(); ++i)
	{
		if (appendNL) lastLines += _T("\r\n");
		lastLines += *i;
		appendNL = true;
	}
	lastLines.shrink_to_fit();
	nm->lpszText = const_cast<TCHAR*>(lastLines.c_str());
	return 0;
}

LRESULT BaseChatFrame::onChatLinkClicked(UINT, WPARAM, LPARAM, BOOL&)
{
	if (!ChatCtrl::g_sSelectedURL.empty())
		WinUtil::openLink(ChatCtrl::g_sSelectedURL);
	return 0;
}

LRESULT BaseChatFrame::onMultilineChatInputButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SET_SETTING(MULTILINE_CHAT_INPUT, !BOOLSETTING(MULTILINE_CHAT_INPUT));
	UpdateLayout();
	checkMultiLine();
	ctrlMessage.SetFocus();
	return 0;
}

void BaseChatFrame::appendChatCtrlItems(OMenu& menu, bool isOp)
{
	if (!ChatCtrl::g_sSelectedIP.empty())
	{
		menu.InsertSeparatorFirst(ChatCtrl::g_sSelectedIP);
#ifdef IRAINMAN_ENABLE_WHOIS
		WinUtil::appendWhoisMenu(menu, ChatCtrl::g_sSelectedIP, false);
		menu.AppendMenu(MF_STRING, IDC_REPORT_CHAT, CTSTRING(DUMP_USER_INFO));
		menu.AppendMenu(MF_SEPARATOR);
#endif
		if (isOp)
		{
			menu.AppendMenu(MF_STRING, IDC_BAN_IP, (_T("!banip ") + ChatCtrl::g_sSelectedIP).c_str());
			menu.AppendMenu(MF_STRING, IDC_UNBAN_IP, (_T("!unban ") + ChatCtrl::g_sSelectedIP).c_str());
			menu.AppendMenu(MF_SEPARATOR);
		}
	}
	
	menu.AppendMenu(MF_STRING, ID_EDIT_COPY, CTSTRING(COPY_SELECTED_TEXT));
	menu.AppendMenu(MF_STRING, IDC_COPY_ACTUAL_LINE,  CTSTRING(COPY_LINE));
	if (!ChatCtrl::g_sSelectedURL.empty())
	{
		menu.AppendMenu(MF_STRING, IDC_COPY_URL, CTSTRING(COPY_LINK));
		
#ifdef IRAINMAN_ENABLE_WHOIS
		if (!Util::isMagnetLink(ChatCtrl::g_sSelectedURL))
		{
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, IDC_WHOIS_URL, (TSTRING(WHO_IS) + _T(" URL: Bgp.He")/* + ChatCtrl::g_sSelectedURL*/).c_str());
		}
#endif
	}
	
	if (!ChatCtrl::g_sSelectedText.empty())
	{
		menu.AppendMenu(MF_SEPARATOR);
		appendInternetSearchItems(menu);
	}
	menu.AppendMenu(MF_SEPARATOR);
	
	menu.AppendMenu(MF_STRING, ID_EDIT_SELECT_ALL, CTSTRING(SELECT_ALL));
	menu.AppendMenu(MF_STRING, ID_EDIT_CLEAR_ALL, CTSTRING(CLEAR));
	menu.AppendMenu(MF_STRING, IDC_SAVE, CTSTRING(SAVE_TO_FILE));
	menu.AppendMenu(MF_SEPARATOR);
	
	menu.AppendMenu(MF_STRING, IDC_AUTOSCROLL_CHAT, CTSTRING(ASCROLL_CHAT));
	if (ctrlClient.getAutoScroll())
		menu.CheckMenuItem(IDC_AUTOSCROLL_CHAT, MF_BYCOMMAND | MF_CHECKED);
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
		File f(path, File::READ, File::OPEN | File::SHARED);
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
		return;
	}
	const bool isUTF8 = buf.compare(0, 3, "\xef\xbb\xbf", 3) == 0;
	const StringTokenizer<string> st(isUTF8 ? buf.substr(3) : buf, "\r\n");
	const StringList& lines = st.getTokens();
	size_t i = lines.size() > (linesCount + 1) ? lines.size() - linesCount : 0;
	ChatCtrl::Message message(nullptr, false, true, Util::emptyStringT, Util::emptyStringT, Colors::g_ChatTextLog, true, false);
	for (; i < lines.size(); ++i)
	{
		message.msg = Text::toT(lines[i]);
		message.msg += _T('\n');
		ctrlClient.appendText(message, 1);
	}
}

int BaseChatFrame::getInputBoxHeight() const
{
	const bool useMultiChat = BOOLSETTING(MULTILINE_CHAT_INPUT) || multiChatLines;
	int lines = useMultiChat ? std::max(multiChatLines + 1, 3u) : 1;
	int height = Fonts::g_fontHeightPixl * lines + 12;
	if (height < MessagePanel::MIN_INPUT_BOX_HEIGHT)
		height = MessagePanel::MIN_INPUT_BOX_HEIGHT;
	return height;
}

OMenu* BaseChatFrame::createUserMenu()
{
	if (!userMenu)
	{
		userMenu = new OMenu;
		userMenu->CreatePopupMenu();
	}
	return userMenu;
}

void BaseChatFrame::destroyUserMenu()
{
	delete userMenu;
	userMenu = nullptr;
}

bool BaseChatFrame::processEnter()
{
	bool insertNewLine = WinUtil::isCtrlOrAlt();
	if (BOOLSETTING(MULTILINE_CHAT_INPUT) && BOOLSETTING(MULTILINE_CHAT_INPUT_BY_CTRL_ENTER))
		insertNewLine = !insertNewLine;
	if (insertNewLine)
		return false;
	onEnter();
	return true;
}

void BaseChatFrame::updateEditHeight()
{
	checkMultiLine();
}

bool BaseChatFrame::processHotKey(int key)
{
	if (key == VK_TAB && GetFocus() == ctrlClient)
	{
		HWND hWndNext = GetNextDlgTabItem(messagePanelHwnd, ctrlClient, WinUtil::isShift());
		if (hWndNext) ::SetFocus(hWndNext);
		return true;
	}
	if (key == VK_ESCAPE)
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
		currentNeedle.clear();
		return true;
	}
	if (((key == VK_F3 && WinUtil::isShift()) || (key == 'F' && WinUtil::isCtrl())) && !WinUtil::isAlt())
	{
		findText(findTextPopup());
		return true;
	}
	if (key == VK_F3)
	{
		findText(currentNeedle.empty() ? findTextPopup() : currentNeedle);
		return true;
	}
	return false;
}

bool BaseChatFrame::handleKey(int key)
{
	if (key == VK_PRIOR)
	{
		ctrlClient.SendMessage(WM_VSCROLL, SB_PAGEUP);
		return true;
	}
	if (key == VK_NEXT)
	{
		ctrlClient.SendMessage(WM_VSCROLL, SB_PAGEDOWN);
		return true;
	}
	return processHotKey(key);
}

void BaseChatFrame::typingNotification()
{
	PLAY_SOUND(SOUND_TYPING_NOTIFY);
}

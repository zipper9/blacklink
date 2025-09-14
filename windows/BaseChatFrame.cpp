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
#include "MainFrm.h"
#include "BrowseFile.h"
#include "NotifUtil.h"
#include "ConfUI.h"
#include "../client/GlobalState.h"
#include "../client/SettingsManager.h"
#include "../client/StringTokenizer.h"
#include "../client/PathUtil.h"
#include "../client/Util.h"
#include "../client/ConfCore.h"
#include <tom.h>
#include <comdef.h>

#ifdef BL_UI_FEATURE_BB_CODES
class DlgInsertLink : public CDialogImpl<DlgInsertLink>
{
		CEdit ctrlLinkText;
		CEdit ctrlDescription;

	public:
		tstring linkText;
		tstring description;
		bool editMode = false;

		enum { IDD = IDD_INSERT_LINK };

		BEGIN_MSG_MAP(DlgInsertLink)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_HANDLER(IDC_LINK_TEXT, EN_CHANGE, onChange)
		END_MSG_MAP()

		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};

static const WinUtil::TextItem texts[] =
{
	{ IDC_CAPTION_LINK_TEXT,   ResourceManager::INSERT_LINK_URL         },
	{ IDC_CAPTION_DESCRIPTION, ResourceManager::INSERT_LINK_DESCRIPTION },
	{ IDOK,                    ResourceManager::OK                      },
	{ IDCANCEL,                ResourceManager::CANCEL                  },
	{ 0,                       ResourceManager::Strings()               }
};

LRESULT DlgInsertLink::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(editMode ? CTSTRING(INSERT_LINK_EDIT) : CTSTRING(INSERT_LINK_INSERT));
	HICON dialogIcon = g_iconBitmaps.getIcon(IconBitmaps::EDITOR_LINK, 0);
	SetIcon(dialogIcon, FALSE);
	SetIcon(dialogIcon, TRUE);
	WinUtil::translate(m_hWnd, texts);

	ctrlLinkText.Attach(GetDlgItem(IDC_LINK_TEXT));
	ctrlLinkText.SetWindowText(linkText.c_str());
	ctrlDescription.Attach(GetDlgItem(IDC_DESCRIPTION));
	ctrlDescription.SetWindowText(description.c_str());

	CenterWindow();
	return TRUE;
}

LRESULT DlgInsertLink::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::getWindowText(ctrlLinkText, linkText);
		WinUtil::getWindowText(ctrlDescription, description);
	}
	EndDialog(wID);
	return 0;
}

LRESULT DlgInsertLink::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring tmp;
	WinUtil::getWindowText(ctrlLinkText, tmp);
	GetDlgItem(IDOK).EnableWindow(!tmp.empty());
	return 0;
}

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

BaseChatFrame::BaseChatFrame() :
	multiChatLines(0),
	msgPanel(nullptr),
	messagePanelHwnd(nullptr),
	messagePanelRect{},
	lastMessageSelPos(0),
	userMenu(nullptr),
	disableChat(false),
	shouldRestoreStatusText(true),
	findDlg(nullptr)
{
	auto ss = SettingsManager::instance.getUiSettings();
	showTimestamps = ss->getBool(Conf::CHAT_TIME_STAMPS);
}

void BaseChatFrame::insertBBCode(WORD wID, HWND hwndCtl)
{
#ifdef BL_UI_FEATURE_BB_CODES
	tstring startTag;
	tstring endTag;
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
			auto ss = SettingsManager::instance.getUiSettings();
			CHOOSECOLOR cc = { sizeof(cc) };
			cc.hwndOwner = ctrlMessage.m_hWnd;
			cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK;
			cc.rgbResult = ss->getInt(Conf::TEXT_GENERAL_FORE_COLOR);
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

LRESULT BaseChatFrame::onInsertLink(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
#ifdef BL_UI_FEATURE_BB_CODES
	int insertPos = 0, endSel = 0;
	ctrlMessage.GetSel(insertPos, endSel);
	tstring text;
	WinUtil::getWindowText(ctrlMessage, text);
	ChatTextParser textParser;
	textParser.initSearch();
	textParser.parseText(text, Colors::charFormat[Colors::TEXT_STYLE_MY_MESSAGE], true, 0);
	DlgInsertLink dlg;
	for (const auto& tag : textParser.getTags())
		if (tag.type == ChatTextParser::BBCODE_URL && insertPos >= (int) tag.openTagStart && insertPos <= (int) tag.closeTagEnd)
		{
			tag.getUrl(text, dlg.linkText, dlg.description);
			dlg.editMode = true;
			insertPos = tag.openTagStart;
			text.erase(tag.openTagStart, tag.closeTagEnd - tag.openTagStart);
			break;
		}
	if (dlg.DoModal(messagePanelHwnd) != IDOK) return 0;
	tstring url = _T("[url");
	if (!dlg.description.empty())
	{
		url += _T("=");
		url += dlg.linkText;
		url += _T("]");
		url += dlg.description;
	}
	else
	{
		url += _T("]");
		url += dlg.linkText;
	}
	url += _T("[/url]");
	text.insert(insertPos, url);
	insertPos += url.length();
	ctrlMessage.SetWindowText(text.c_str());
	ctrlMessage.SetSel(insertPos, insertPos);
	ctrlMessage.SetFocus();
#endif
	return 0;
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

void BaseChatFrame::initStatusCtrl()
{
	if (!ctrlStatus)
	{
		ctrlStatus.setAutoGripper(true);
		ctrlStatus.setFont(Fonts::g_systemFont, false);
		ctrlStatus.Create(messagePanelHwnd, 0, nullptr, WS_CHILD);
	}
	if (!ctrlLastLinesToolTip)
	{
		ctrlLastLinesToolTip.Create(ctrlStatus, messagePanelRect, _T("BaseChatFrame_ToolTips"), WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
		ctrlLastLinesToolTip.SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		CToolInfo ti(TTF_SUBCLASS, ctrlStatus, 0, nullptr, LPSTR_TEXTCALLBACK);
		ctrlLastLinesToolTip.AddTool(&ti);
		ctrlLastLinesToolTip.SetDelayTime(TTDT_AUTOPOP, 15000);
	}
}

void BaseChatFrame::createChatCtrl()
{
	if (ctrlClient) return;
	HWND hwnd = ctrlClient.Create(messagePanelHwnd, messagePanelRect, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                              WS_TABSTOP | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL | ES_MULTILINE | ES_SAVESEL | ES_READONLY | ES_NOOLEDRAGDROP, WS_EX_STATICEDGE, IDC_CLIENT);
	if (!hwnd)
	{
		dcdebug("Error create BaseChatFrame::createChatCtrl %s", Util::translateError().c_str());
		dcassert(0);
		return;
	}
	ctrlClient.SetTabStops(120);
	ctrlClient.LimitText(0);
	ctrlClient.SetFont(Fonts::g_font);
	ctrlClient.SetAutoURLDetect(FALSE);
	ctrlClient.SetEventMask(ctrlClient.GetEventMask() | ENM_LINK);
	ctrlClient.SetBackgroundColor(Colors::g_bgColor);
	ctrlClient.SetUndoLimit(0);
	if (!disableChat)
		readFrameLog();
}

void BaseChatFrame::createMessageCtrl(ATL::CMessageMap* messageMap, DWORD messageMapID)
{
	if (ctrlMessage) return;
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
	if (disableChat)
	{
		ctrlMessage.SetWindowText(CTSTRING(CHAT_DISABLED_NOTICE));
		ctrlMessage.EnableWindow(FALSE);
	}
}

void BaseChatFrame::destroyMessageCtrl(bool isShutdown)
{
	dcassert(!msgPanel); // destroyMessagePanel must have been called before
	if (ctrlMessage.m_hWnd)
	{
		if (!isShutdown)
		{
			WinUtil::getWindowText(ctrlMessage, lastMessage);
			lastMessageSelPos = ctrlMessage.GetSel();
		}
		ctrlMessage.DestroyWindow();
	}
}

void BaseChatFrame::createMessagePanel(bool showSelectHubButton, bool showCCPMButton)
{
	dcassert(!GlobalState::isShuttingDown());
	if (!msgPanel && !GlobalState::isStartingUp())
	{
		msgPanel = new MessagePanel(ctrlMessage);
		msgPanel->showSelectHubButton = showSelectHubButton;
		msgPanel->showCCPMButton = showCCPMButton;
		msgPanel->disableChat = disableChat;
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

void BaseChatFrame::setStatusText(int index, const tstring& text)
{
	dcassert(!GlobalState::isShuttingDown());
	dcassert(index < ctrlStatus.getNumPanes());
	if (index >= ctrlStatus.getNumPanes()) return;
	ctrlStatus.setPaneText(index, text);
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

TCHAR BaseChatFrame::getNickDelimiter()
{
	auto ss = SettingsManager::instance.getUiSettings();
	return ss->getBool(Conf::CHAT_REFFERING_TO_NICK) ? _T(',') : _T(':');
}

void BaseChatFrame::clearInputBox()
{
	if (ctrlMessage)
		ctrlMessage.SetWindowText(_T(""));
	multiChatLines = 0;
}

LRESULT BaseChatFrame::onPerformWebSearch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string query;
	switch (getWebSearchType(wID))
	{
		case SearchUrl::KEYWORD:
			query = Text::fromT(ChatCtrl::g_sSelectedText);
			break;
		case SearchUrl::HOSTNAME:
			query = ChatCtrl::g_sSelectedHostname;
			break;
		case SearchUrl::IP4:
		case SearchUrl::IP6:
			query = Text::fromT(ChatCtrl::g_sSelectedIP);
	}
	performWebSearch(wID, query);
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

LRESULT BaseChatFrame::onTextTranscode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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
	else if (!disableChat && ctrlMessage && hWnd == ctrlMessage.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT BaseChatFrame::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	checkMultiLine();
	onTextEdited();
	return 0;
}

LRESULT BaseChatFrame::onForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	LPMSG pMsg = (LPMSG)lParam;
	if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST && ctrlLastLinesToolTip)
		ctrlLastLinesToolTip.RelayEvent(pMsg);
	return 0;
}

bool BaseChatFrame::processFrameCommand(const Commands::ParsedCommand& pc, Commands::Result& res)
{
	switch (pc.command)
	{
		case Commands::COMMAND_CLEAR:
		{
			if (ctrlClient.IsWindow())
				ctrlClient.Clear();
			res.what = Commands::RESULT_NO_TEXT;
			return true;
		}
		case Commands::COMMAND_TIMESTAMPS:
			showTimestamps = !showTimestamps;
			res.text = showTimestamps ? STRING(TIMESTAMPS_ENABLED) : STRING(TIMESTAMPS_DISABLED);
			res.what = Commands::RESULT_LOCAL_TEXT;
			return true;
		case Commands::COMMAND_FIND_TEXT:
		{
			res.what = Commands::RESULT_NO_TEXT;
			tstring param;
			if (pc.args.size() >= 2)
			{
				param = Text::toT(pc.args[1]);
				ctrlClient.SetSelNone();
				ctrlClient.resetFindPos();
				ctrlClient.setNeedle(param);
				ctrlClient.setFindFlags(FR_DOWN);
				ctrlClient.findText();
			}
			else
				showFindDialog();
			return true;
		}
		case Commands::COMMAND_SAY:
		case Commands::COMMAND_ME:
			if (pc.args.size() >= 2)
				sendMessage(pc.args[1], pc.command == Commands::COMMAND_ME);
			return true;
	}
	if (MainFrame::getMainFrame()->processCommand(pc, res)) return true;
	return Commands::processCommand(pc, res);
}

void BaseChatFrame::onEnter()
{
	bool updateLayout = false;
	bool resetInputMessageText = true;
	Commands::ParsedCommand pc;
	pc.command = -1;
	dcassert(ctrlMessage);
	if (ctrlMessage && ctrlMessage.GetWindowTextLength() > 0)
	{
		tstring fullMessageText;
		WinUtil::getWindowText(ctrlMessage, fullMessageText);
		ctrlMessage.saveCommand(fullMessageText);

		// Process commands
		if (fullMessageText[0] == _T('/'))
		{
			bool result = Commands::parseCommand(Text::fromT(fullMessageText), pc);
			if (!result)
			{
				auto ss = SettingsManager::instance.getUiSettings();
				if (ss->getBool(Conf::SEND_UNKNOWN_COMMANDS))
					sendMessage(Text::fromT(fullMessageText));
				else
				{
					tstring msg = TSTRING(UNKNOWN_COMMAND);
					if (!pc.args.empty())
						msg += _T(" /") + Text::toT(pc.args[0]);
					addStatus(msg);
				}
			}
			pc.frameId = frameId;
		}
		else
			processFrameMessage(fullMessageText, resetInputMessageText);
		if (resetInputMessageText)
		{
			clearInputBox();
			updateLayout = true;
		}
	}
	else
		MessageBeep(MB_ICONEXCLAMATION);

	if (pc.command != -1)
	{
		Commands::Result res;
		if (processFrameCommand(pc, res))
			sendCommandResult(res);
	}

	if (updateLayout)
		UpdateLayout();
}

static void appendMyNick(string& text, bool thirdPerson)
{
	if (thirdPerson)
	{
		auto ss = SettingsManager::instance.getCoreSettings();
		ss->lockRead();
		string nickText = ChatMessage::formatNick(ss->getString(Conf::NICK), true);
		ss->unlockRead();
		text.insert(0, nickText);
	}
}

void BaseChatFrame::sendCommandResult(Commands::Result& res)
{
	bool thirdPerson = false;
	ChatMessage::translateMe(res.text, thirdPerson);
	switch (res.what)
	{
		case Commands::RESULT_TEXT:
			sendMessage(res.text, thirdPerson);
			break;

		case Commands::RESULT_LOCAL_TEXT:
			appendMyNick(res.text, thirdPerson);
			addSystemMessage(Text::toT(res.text), Colors::TEXT_STYLE_SYSTEM_MESSAGE);
			break;

		case Commands::RESULT_ERROR_MESSAGE:
			appendMyNick(res.text, thirdPerson);
			addStatus(Text::toT(res.text), true, false);
	}
}

LRESULT BaseChatFrame::onWinampSpam(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	auto ss = SettingsManager::instance.getUiSettings();
	const char* cmd;
	switch (ss->getInt(Conf::MEDIA_PLAYER))
	{
		case Conf::WinAmp:
			cmd = "winamp";
			break;
		case Conf::WinMediaPlayer:
			cmd = "wmp";
			break;
		case Conf::iTunes:
			cmd = "itunes";
			break;
		case Conf::WinMediaPlayerClassic:
			cmd = "mpc";
			break;
		case Conf::JetAudio:
			cmd = "ja";
			break;
		default:
			addStatus(TSTRING(NO_MEDIA_SPAM));
			return 0;
	}
	Commands::ParsedCommand pc;
	pc.args.push_back(cmd);
	pc.command = Commands::COMMAND_MEDIA_PLAYER;
	Commands::Result res;
	if (MainFrame::getMainFrame()->processCommand(pc, res))
		sendCommandResult(res);
	return 0;
}

void BaseChatFrame::addStatus(const tstring& line, bool inChat /*= true*/, bool history /*= true*/, int textStyle /* = Colors::TEXT_STYLE_SYSTEM_MESSAGE*/)
{
	ASSERT_MAIN_THREAD();
	dcassert(!GlobalState::isShuttingDown());
	if (GlobalState::isShuttingDown())
		return;
	tstring formattedLine = _T('[') + Text::toT(Util::getShortTimeString()) + _T("] ") + line;
	if (formattedLine.length() > 512)
		formattedLine.resize(512);
	setStatusText(0, formattedLine);

	if (history)
		statusHistory.addLine(formattedLine);

	if (inChat && SettingsManager::instance.getUiSettings()->getBool(Conf::STATUS_IN_CHAT))
		addSystemMessage(line, textStyle);
}

void BaseChatFrame::addSystemMessage(const tstring& line, int textStyle)
{
	addLine(_T("*** ") + line, 0, textStyle);
}

void BaseChatFrame::addLine(const tstring& line, unsigned maxSmiles, int textStyle)
{
	if (disableChat)
		return;
#ifdef _DEBUG
	if (line.find(_T("&#124")) != tstring::npos)
	{
		dcassert(0);
	}
#endif
	if (showTimestamps)
	{
		const ChatCtrl::Message message(nullptr, false, true, _T('[') + Text::toT(Util::getShortTimeString()) + _T("] "), line, textStyle, false);
		ctrlClient.appendText(message, maxSmiles, true);
	}
	else
	{
		const ChatCtrl::Message message(nullptr, false, true, Util::emptyStringT, line, textStyle, false);
		ctrlClient.appendText(message, maxSmiles, true);
	}
}

void BaseChatFrame::addLine(const Identity& from, bool myMessage, bool thirdPerson, const tstring& line, unsigned maxSmiles, int textStyle, string& extra)
{
	if (disableChat)
		return;
	if (ctrlClient.IsWindow())
		ctrlClient.adjustTextSize();
	string additionalInfo;
	if (showTimestamps) additionalInfo = Util::getShortTimeString();
	extra = ChatMessage::getExtra(from);
	if (!extra.empty())
	{
		if (!additionalInfo.empty()) additionalInfo += " | ";
		additionalInfo += extra;
	}
	if (!additionalInfo.empty())
	{
		additionalInfo.insert(0, "[");
		additionalInfo += "] ";
	}
	const ChatCtrl::Message message(&from, myMessage, thirdPerson, Text::toT(additionalInfo), line, textStyle, true);
	ctrlClient.appendText(message, maxSmiles, true);
}

LRESULT BaseChatFrame::onGetToolTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	statusHistory.getToolTip(pnmh);
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
	auto ss = SettingsManager::instance.getUiSettings();
	ss->setBool(Conf::MULTILINE_CHAT_INPUT, !ss->getBool(Conf::MULTILINE_CHAT_INPUT));
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
		appendWebSearchItems(menu, SearchUrl::IP4, false, ResourceManager::Strings());
		menu.AppendMenu(MF_STRING, IDC_REPORT_CHAT, CTSTRING(DUMP_USER_INFO));
		menu.AppendMenu(MF_SEPARATOR);
		if (isOp)
		{
			menu.AppendMenu(MF_STRING, IDC_BAN_IP, (_T("!banip ") + ChatCtrl::g_sSelectedIP).c_str());
			menu.AppendMenu(MF_STRING, IDC_UNBAN_IP, (_T("!unban ") + ChatCtrl::g_sSelectedIP).c_str());
			menu.AppendMenu(MF_SEPARATOR);
		}
	}

	menu.AppendMenu(MF_STRING, ID_EDIT_COPY, CTSTRING(COPY_SELECTED_TEXT));
	menu.AppendMenu(MF_STRING, IDC_COPY_ACTUAL_LINE,  CTSTRING(COPY_LINE));
	ChatCtrl::g_sSelectedHostname.clear();
	if (!ChatCtrl::g_sSelectedURL.empty())
	{
		menu.AppendMenu(MF_STRING, IDC_COPY_URL, CTSTRING(COPY_LINK));
		if (!Util::isMagnetLink(ChatCtrl::g_sSelectedURL))
		{
			Util::ParsedUrl url;
			Util::decodeUrl(Text::fromT(ChatCtrl::g_sSelectedURL), url);
			if (!url.host.empty())
			{
				ChatCtrl::g_sSelectedHostname = std::move(url.host);
				appendWebSearchItems(menu, SearchUrl::HOSTNAME, false, ResourceManager::Strings());
			}
		}
	}

	if (!ChatCtrl::g_sSelectedText.empty())
		appendWebSearchItems(menu, SearchUrl::KEYWORD, true, ResourceManager::WEB_SEARCH_KEYWORD);
	menu.AppendMenu(MF_SEPARATOR);

	menu.AppendMenu(MF_STRING, ID_EDIT_SELECT_ALL, CTSTRING(SELECT_ALL), g_iconBitmaps.getBitmap(IconBitmaps::SELECTION, 0));
	menu.AppendMenu(MF_STRING, ID_EDIT_CLEAR_ALL, CTSTRING(CLEAR), g_iconBitmaps.getBitmap(IconBitmaps::ERASE, 0));
	menu.AppendMenu(MF_STRING, IDC_SAVE, CTSTRING(SAVE_TO_FILE));
	menu.AppendMenu(MF_SEPARATOR);

	menu.AppendMenu(MF_STRING, IDC_AUTOSCROLL_CHAT, CTSTRING(ASCROLL_CHAT));
	if (ctrlClient.getAutoScroll())
		menu.CheckMenuItem(IDC_AUTOSCROLL_CHAT, MF_BYCOMMAND | MF_CHECKED);
}

void BaseChatFrame::appendNickToChat(const tstring& nick)
{
	dcassert(ctrlMessage);
	if (!ctrlMessage) return;

	tstring text;
	WinUtil::getWindowText(ctrlMessage, text);

	size_t lastPos = tstring::npos;
	size_t i = 0;
	size_t len = text.length();
	const TCHAR* ts = text.c_str();
	while (i < len)
	{
		if (ts[i] == ' ' || ts[i] == '\t' || ts[i] == '\r' || ts[i] == '\n' || ts[i] == '<')
		{
			i++;
			continue;
		}
		size_t j = text.find_first_of(ChatCtrl::nickBoundaryChars, i);
		if (j == tstring::npos) j = len;
		if (i < j)
		{
			tstring oldNick = text.substr(i, j-i);
			if (oldNick == nick) return;
			if (hasNick(oldNick))
				lastPos = j;
			else
				break;
		}
		i = j + 1;
	}
	tstring insText = nick;
	int cursorPos;
	if (lastPos != tstring::npos)
	{
		insText.insert(0, _T(", "));
		cursorPos = 0;
		if (ts[lastPos] != ':' && ts[lastPos] != ',')
			insText += getNickDelimiter();
		else
			cursorPos++;
		text.insert(lastPos, insText);
		cursorPos += lastPos + insText.length();
	}
	else
	{
		insText += getNickDelimiter();
		insText += _T(' ');
		text.insert(0, insText);
		cursorPos = insText.length();
	}
	ctrlMessage.SetWindowText(text.c_str());
	ctrlMessage.SetSel(cursorPos, cursorPos);
	ctrlMessage.SetFocus();
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
	ChatCtrl::Message message(nullptr, false, true, Util::emptyStringT, Util::emptyStringT, Colors::TEXT_STYLE_LOG, true, false);
	for (; i < lines.size(); ++i)
	{
		message.msg = Text::toT(lines[i]);
		message.msg += _T('\n');
		ctrlClient.appendText(message, UINT_MAX, false);
	}
}

int BaseChatFrame::getInputBoxHeight() const
{
	const bool useMultiChat = SettingsManager::instance.getUiSettings()->getBool(Conf::MULTILINE_CHAT_INPUT) || multiChatLines;
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
	bool insertNewLine = WinUtil::isCtrl() || WinUtil::isAlt();
	auto ss = SettingsManager::instance.getUiSettings();
	if (ss->getBool(Conf::MULTILINE_CHAT_INPUT) && ss->getBool(Conf::MULTILINE_CHAT_INPUT_BY_CTRL_ENTER))
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
		return true;
	}
	if (((key == VK_F3 && WinUtil::isShift()) || (key == 'F' && WinUtil::isCtrl())) && !WinUtil::isAlt())
	{
		if (!ctrlClient.getNeedle().empty())
			findNext();
		else
			showFindDialog();
		return true;
	}
	if (key == VK_F3)
	{
		showFindDialog();
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

void BaseChatFrame::setChatDisabled(bool disabled)
{
	if (disableChat == disabled) return;
	disableChat = disabled;
	if (!ctrlMessage) return;
	if (disabled)
	{
		ctrlMessage.SetWindowText(CTSTRING(CHAT_DISABLED_NOTICE));
		ctrlMessage.EnableWindow(FALSE);
	}
	else
	{
		ctrlMessage.SetWindowText(_T(""));
		ctrlMessage.EnableWindow(TRUE);
	}
	if (msgPanel)
		msgPanel->disableChat = disabled;
}

void BaseChatFrame::showFindDialog()
{
	if (findDlg)
	{
		findDlg->SetActiveWindow();
		findDlg->ShowWindow(SW_SHOW);
		return;
	}
	findDlg = new CFindReplaceDialog;
	HWND hWndDlg = findDlg->Create(TRUE,
		ctrlClient.getNeedle().c_str(), nullptr,
		ctrlClient.getFindFlags(), messagePanelHwnd);
	if (!hWndDlg)
	{
		delete findDlg;
		findDlg = nullptr;
	}
	else
	{
		findDlg->SetActiveWindow();
		findDlg->ShowWindow(SW_SHOW);
	}
}

LRESULT BaseChatFrame::onFindDialogMessage(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (!findDlg)
	{
		dcassert(0);
		::MessageBeep(MB_ICONERROR);
		return 1;
	}

	FINDREPLACE* findReplace = reinterpret_cast<FINDREPLACE*>(lParam);
	if (findReplace)
	{
		if (findDlg->FindNext())
		{
			ctrlClient.setNeedle(findDlg->GetFindString());
			ctrlClient.setFindFlags(findDlg->m_fr.Flags);
			findNext();
		}
		else if (findDlg->IsTerminating())
			findDlg = nullptr;
	}
	return 0;
}

void BaseChatFrame::findNext()
{
	if (ctrlClient.findText())
	{
		if (findDlg) adjustFindDlgPosition(findDlg->m_hWnd);
	}
	else
		searchStringNotFound();
}

void BaseChatFrame::searchStringNotFound()
{
	addStatus(TSTRING(STRING_NOT_FOUND) + _T(' ') + ctrlClient.getNeedle(), false, false, Colors::TEXT_STYLE_SYSTEM_MESSAGE);
}

void BaseChatFrame::themeChanged()
{
	ctrlClient.SetBackgroundColor(Colors::g_bgColor);
	int len = ctrlClient.GetWindowTextLength();
	if (!len) return;
	CHARRANGE cr;
	ctrlClient.GetSel(cr);
	ctrlClient.SetSel(0, -1);
	CHARFORMAT2 cf = Colors::charFormat[Colors::TEXT_STYLE_LOG];
	ctrlClient.SetSelectionCharFormat(cf);
	ctrlClient.SetSel(cr);
	addStatus(TSTRING(CHAT_THEME_CHANGED_MSG), true, false);
}

// Copied from atlfind.h
void BaseChatFrame::adjustFindDlgPosition(HWND hWnd)
{
	FINDTEXTEX ft = {};
	ctrlClient.GetSel(ft.chrg);
	POINT pt = ctrlClient.PosFromChar(ft.chrg.cpMin);
	::ClientToScreen(ctrlClient.GetParent(), &pt);
	RECT rc;
	::GetWindowRect(hWnd, &rc);
	if (PtInRect(&rc, pt))
	{
		if (pt.y > rc.bottom - rc.top)
			OffsetRect(&rc, 0, pt.y - rc.bottom - 20);
		else
		{
			int vertExt = GetSystemMetrics(SM_CYSCREEN);
			if (pt.y + rc.bottom - rc.top < vertExt)
				OffsetRect(&rc, 0, 40 + pt.y - rc.top);
		}
		::MoveWindow(hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	}
}

BOOL BaseChatFrame::isFindDialogMessage(MSG* msg) const
{
	return findDlg && findDlg->IsDialogMessage(msg);
}

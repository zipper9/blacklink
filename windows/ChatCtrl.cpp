/*
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
#include "ChatCtrl.h"
#include "WinUtil.h"
#include "Colors.h"
#include "NotifUtil.h"
#include "../client/OnlineUser.h"
#include "../client/Client.h"
#include "../client/Util.h"
#include "../client/GlobalState.h"
#include <boost/algorithm/string.hpp>
#include <tom.h>

#ifdef BL_UI_FEATURE_EMOTICONS
#include "Emoticons.h"
#include "../GdiOle/GDIImageOle.h"
#include "../client/SysVersion.h"
#include "ImageDataObject.h"
#include "MainFrm.h"
#endif

extern "C" const GUID IID_ITextDocument =
{
	0x8CC497C0, 0xA1DF, 0x11CE, { 0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D }
};

const tstring ChatCtrl::nickBoundaryChars(_T("\r\n \"<>,.:;()!?*%+-"));

tstring ChatCtrl::g_sSelectedText;
tstring ChatCtrl::g_sSelectedIP;
tstring ChatCtrl::g_sSelectedUserName;
tstring ChatCtrl::g_sSelectedURL;
string ChatCtrl::g_sSelectedHostname;

ChatCtrl::ChatCtrl() : autoScroll(true), useChatCacheFlag(true), chatCacheSize(0),
	ignoreLinkStart(0), ignoreLinkEnd(0), selectedLine(-1), pRichEditOle(nullptr),
	findInit(true), findRangeStart(0), findRangeEnd(0), currentFindFlags(FR_DOWN)
#ifdef BL_UI_FEATURE_EMOTICONS
	, totalEmoticons(0), pStorage(nullptr), refs(0)
#endif
{
}

ChatCtrl::~ChatCtrl()
{
	if (pRichEditOle)
		pRichEditOle->Release();
#ifdef BL_UI_FEATURE_EMOTICONS
	if (pStorage)
		pStorage->Release();
#endif
}

void ChatCtrl::adjustTextSize()
{
	int maxLines = SettingsManager::instance.getUiSettings()->getInt(Conf::CHAT_BUFFER_SIZE);
	if (maxLines <= 0) return;
	int lineCount = GetLineCount();
	if (lineCount > maxLines)
	{
		dcdebug("Removing %d lines\n", lineCount - maxLines);
		int pos = LineIndex(lineCount - maxLines);
		SetSel(0, pos);
		ReplaceSel(_T(""));
	}
}

static void normalizeLineBreaks(tstring& text)
{
	boost::replace_all(text, _T("\r\n"), _T("\n"));
	boost::replace_all(text, _T("\n\r"), _T("\n"));
	std::replace(text.begin(), text.end(), _T('\r'), _T('\n'));
}

ChatCtrl::Message::Message(const Identity* id, bool myMessage, bool thirdPerson, const tstring& extra, const tstring& text, int textStyle, bool useEmoticons, bool removeLineBreaks /*= true*/) :
	nick(id ? Text::toT(id->getNick()) : Util::emptyStringT), myMessage(myMessage),
	isRealUser(id ? !id->isBotOrHub() : false), msg(text), textStyle(textStyle),
	thirdPerson(thirdPerson), extra(extra), useEmoticons(useEmoticons)
{
	dcassert(!GlobalState::isShuttingDown());
	if (!GlobalState::isShuttingDown())
	{
		if (removeLineBreaks)
			normalizeLineBreaks(msg);
		msg += _T('\n');

#ifdef BL_UI_FEATURE_EMOTICONS
		if (emoticonPackList.getPacks().empty() || SysVersion::isWine())
			useEmoticons = false;
#endif

		isFavorite = isBanned = isOp = false;
		if (id)
		{
			isOp = id->isOp();
			if (!thirdPerson && !myMessage)
			{
				auto flags = id->getUser()->getFlags();
				if (flags & User::FAVORITE)
				{
					isFavorite = true;
					if (flags & User::BANNED) isBanned = true;
				}
			}
		}
	}
}

void ChatCtrl::restoreChatCache()
{
	ASSERT_MAIN_THREAD();
	CWaitCursor waitCursor;
	std::list<Message> tempChatCache;
	bool flag = false;
	{
		std::swap(useChatCacheFlag, flag);
		tempChatCache.swap(chatCache);
		chatCacheSize = 0;
	}
	int count = tempChatCache.size();
	tstring oldText;
	for (auto i = tempChatCache.begin(); i != tempChatCache.end(); ++i)
	{
		if (GlobalState::isShuttingDown())
			return;
		if (count-- > 40) // Disable styles for old messages
		{
			oldText += _T("* ") + i->extra + _T(' ') + i->nick + _T(' ') + i->msg + _T("\r\n");
			continue;
		}
		if (!oldText.empty())
		{
			LONG selBegin = 0;
			LONG selEnd = 0;
			insertAndFormat(oldText, Colors::getCharFormat(i->textStyle), selBegin, selEnd);
			oldText.clear();
		}
		if (flag)
			appendText(*i, UINT_MAX, false);
	}
}

void ChatCtrl::insertAndFormat(const tstring& text, CHARFORMAT2 cf, LONG& startPos, LONG& endPos, unsigned addFlags)
{
	dcassert(!text.empty());
	if (!text.empty())
	{
		startPos = endPos = GetTextLengthEx(GTL_NUMCHARS);
		SetSel(endPos, endPos);
		ReplaceSel(text.c_str());
		endPos = GetTextLengthEx(GTL_NUMCHARS);
		SetSel(startPos, endPos);
		cf.dwEffects |= addFlags;
		SetSelectionCharFormat(cf);
	}
}

void ChatCtrl::appendText(const Message& message, unsigned maxEmoticons, bool highlightNick)
{
	dcassert(!GlobalState::isShuttingDown());
	if (GlobalState::isShuttingDown())
		return;

	ASSERT_MAIN_THREAD();
	const auto* ss = SettingsManager::instance.getUiSettings();
	if (useChatCacheFlag)
	{
		chatCache.push_back(message);
		chatCacheSize += message.length();
		if (chatCacheSize + 1000 > (size_t) ss->getInt(Conf::CHAT_BUFFER_SIZE))
		{
			chatCacheSize -= chatCache.front().length();
			chatCache.pop_front();
		}
		return;
	}
	LONG selBeginSaved = 0, selEndSaved = 0;
	GetSel(selBeginSaved, selEndSaved);
	POINT cr = { 0 };
	GetScrollPos(&cr);

	SetRedraw(FALSE);
	LONG selBegin = 0;
	LONG selEnd = 0;

	// Insert extra info and format with default style
	if (!message.extra.empty())
	{
		insertAndFormat(message.extra, Colors::charFormat[Colors::TEXT_STYLE_TIMESTAMP], selBegin, selEnd);
		PARAFORMAT2 pf;
		memset(&pf, 0, sizeof(PARAFORMAT2));
		pf.dwMask = PFM_STARTINDENT;
		pf.dxStartIndent = 0;
		SetParaFormat(pf);
	}

	tstring text = message.msg;
	if (!message.nick.empty())
	{
		const CHARFORMAT2& currentCF = Colors::charFormat[
		    message.myMessage ? Colors::TEXT_STYLE_MY_NICK :
		    message.isFavorite ? (message.isBanned ? Colors::TEXT_STYLE_BANNED_USER : Colors::TEXT_STYLE_FAV_USER) :
		    message.isOp ? Colors::TEXT_STYLE_OP : Colors::TEXT_STYLE_OTHER_USER];
		const CHARFORMAT2& cf = Colors::getCharFormat(message.textStyle);
		const bool boldMsgAuthor = ss->getBool(Conf::BOLD_MSG_AUTHOR);
		if (message.thirdPerson)
		{
			static const tstring strStar = _T("* ");
			insertAndFormat(strStar, cf, selBegin, selEnd);
			insertAndFormat(message.nick, currentCF, selBegin, selEnd, boldMsgAuthor ? CFE_BOLD : 0);
			insertAndFormat(_T(" "), cf, selBegin, selEnd);
		}
		else
		{
			static const tstring strOpen = _T("<");
			static const tstring strClose = _T("> ");
			insertAndFormat(strOpen, cf, selBegin, selEnd);
			insertAndFormat(message.nick, currentCF, selBegin, selEnd, boldMsgAuthor ? CFE_BOLD : 0);
			insertAndFormat(strClose, cf, selBegin, selEnd);
		}
	}

	appendTextInternal(text, message, maxEmoticons, highlightNick);
	SetSel(selBeginSaved, selEndSaved);
	goToEnd(cr, false);
	SetRedraw(TRUE);
	RedrawWindow(NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void ChatCtrl::appendTextInternal(tstring& text, const Message& message, unsigned maxEmoticons, bool highlightNick)
{
	dcassert(!GlobalState::isShuttingDown());
	if (!GlobalState::isShuttingDown())
		appendText(text, message, maxEmoticons, highlightNick);
}

void ChatCtrl::appendTextInternal(tstring&& text, const Message& message, unsigned maxEmoticons, bool highlightNick)
{
	dcassert(!GlobalState::isShuttingDown());
	if (!GlobalState::isShuttingDown())
		appendText(text, message, maxEmoticons, highlightNick);
}

void ChatCtrl::appendText(tstring& text, const Message& message, unsigned maxEmoticons, bool highlightNick)
{
	const auto* ss = SettingsManager::instance.getUiSettings();
	const auto& cf = message.myMessage ? Colors::charFormat[Colors::TEXT_STYLE_MY_MESSAGE] : Colors::getCharFormat(message.textStyle);
#ifdef BL_UI_FEATURE_BB_CODES
	const bool formatBBCodes = ss->getBool(Conf::FORMAT_BB_CODES) && (message.isRealUser || ss->getBool(Conf::FORMAT_BOT_MESSAGE));
#else
	const bool formatBBCodes = false;
#endif
	chatTextParser.parseText(text, cf, formatBBCodes, maxEmoticons);
	chatTextParser.processText(text);

	LONG startPos, endPos;
	insertAndFormat(text, cf, startPos, endPos);

#ifdef BL_UI_FEATURE_BB_CODES
	bool acceptColors = ss->getBool(Conf::FORMAT_BB_CODES_COLORS);
	for (auto& ti : chatTextParser.getTags())
	{
		if (ti.openTagStart == tstring::npos || ti.closeTagEnd == tstring::npos) continue;
		if (ti.type == ChatTextParser::BBCODE_COLOR && !acceptColors) continue;
		if (ti.type == ChatTextParser::BBCODE_URL) continue;
		SetSel(startPos + ti.openTagStart, startPos + ti.closeTagEnd);
		SetSelectionCharFormat(ti.fmt);
	}
#endif

	for (const auto& li : chatTextParser.getLinks())
	{
		if (li.start == tstring::npos || li.end == tstring::npos) continue;
		auto tmp = Colors::charFormat[Colors::TEXT_STYLE_URL];
		SetSel(startPos + li.start, startPos + li.end - li.hiddenTextLen);
		SetSelectionCharFormat(tmp);
		if (li.hiddenTextLen)
		{
			tmp.dwMask |= CFM_HIDDEN;
			tmp.dwEffects |= CFE_HIDDEN;
			SetSel(startPos + li.end - li.hiddenTextLen, startPos + li.end);
			SetSelectionCharFormat(tmp);
		}
	}

#ifdef BL_UI_FEATURE_EMOTICONS
	HWND mainFrameWnd = MainFrame::getMainFrame()->m_hWnd;
	for (const auto& ei : chatTextParser.getEmoticons())
	{
		SetSel(startPos + ei.start, startPos + ei.end);
		ReplaceSel(Util::emptyStringT.c_str());
		IOleClientSite *pOleClientSite = nullptr;
		if (!pRichEditOle)
			initRichEditOle();

		if (pRichEditOle && SUCCEEDED(pRichEditOle->GetClientSite(&pOleClientSite)) && pOleClientSite)
		{
			IOleObject* pObject = ei.emoticon->getImageObject(
				ss->getBool(Conf::CHAT_ANIM_SMILES) ? Emoticon::FLAG_PREFER_GIF : 0,
				pOleClientSite, pStorage, mainFrameWnd, WM_ANIM_CHANGE_FRAME,
				Colors::charFormat[message.myMessage ? Colors::TEXT_STYLE_MY_MESSAGE : Colors::TEXT_STYLE_NORMAL].crBackColor,
				ei.emoticon->getText());
			if (pObject)
			{
				if (CImageDataObject::InsertObject(m_hWnd, pRichEditOle, pOleClientSite, pStorage, pObject))
					totalEmoticons++;
				pObject->Release();
			}
			pOleClientSite->Release();
		}
	}
#endif

	if (!myNick.empty() && highlightNick)
	{
		bool nickFound = false;
		size_t currentLink = 0;
		tstring::size_type pos = 0;
		while (true)
		{
			chatTextParser.findSubstringAvoidingLinks(pos, text, myNick, currentLink);
			if (pos == tstring::npos) break;
			if ((pos == 0 || nickBoundaryChars.find(text[pos-1]) != tstring::npos) &&
			    (pos + myNick.length() >= text.length() || nickBoundaryChars.find(text[pos + myNick.length()]) != tstring::npos))
			{
				SetSel(startPos + pos, startPos + pos + myNick.length());
				auto tmp = Colors::charFormat[Colors::TEXT_STYLE_MY_NICK];
				SetSelectionCharFormat(tmp);
				nickFound = true;
				pos += myNick.length() + 1;
			}
			else
				++pos;
		}
		if (nickFound)
			PLAY_SOUND(SOUND_CHATNAMEFILE);
	}
	chatTextParser.clear();
}

bool ChatCtrl::hitNick(POINT p, tstring& nick, int& startPos, int& endPos)
{
	static const int MAX_NICK_LEN = 64;
	if (hubHint.empty()) return false;

	int charPos = CharFromPos(p);
	int lineIndex = LineFromChar(charPos);
	int lineStart = LineIndex(lineIndex);
	int lineEnd = lineStart + LineLength(charPos);

	if (!(charPos >= lineStart && charPos < lineEnd)) return false;

	if (charPos - MAX_NICK_LEN > lineStart) lineStart = charPos - MAX_NICK_LEN;
	if (charPos + MAX_NICK_LEN < lineEnd) lineEnd = charPos + MAX_NICK_LEN;

	tstring line;
	int len = lineEnd - lineStart;
	line.resize(len);
	GetTextRange(lineStart, lineEnd, &line[0]);
	tstring::size_type pos = charPos - lineStart;
	tstring::size_type nickStart = line.find_last_of(nickBoundaryChars, pos);
	if (nickStart == tstring::npos || nickStart == pos) return false;
	tstring::size_type nickEnd = line.find_first_of(nickBoundaryChars, pos + 1);
	if (nickEnd == tstring::npos) nickEnd = line.length();
	nick = line.substr(nickStart + 1, nickEnd - (nickStart + 1));
	if (!ClientManager::findLegacyUser(Text::fromT(nick), hubHint)) return false;

	startPos = lineStart + nickStart + 1;
	endPos = lineStart + nickEnd;
	return true;
}

bool ChatCtrl::hitIP(POINT p, tstring& result, int& startPos, int& endPos)
{
	// TODO: add IPv6 support.
	const int charPos = CharFromPos(p);
	int len = LineLength(charPos) + 1;
	if (len < 7) // 1.1.1.1
		return false;

	DWORD posBegin = FindWordBreak(WB_LEFT, charPos);
	DWORD posEnd = FindWordBreak(WB_RIGHTBREAK, charPos);
	len = (int) posEnd - (int) posBegin;

	if (len < 7) // 1.1.1.1
		return false;
		
	tstring text;
	text.resize(len);
	GetTextRange(posBegin, posEnd, &text[0]);
	
	for (size_t i = 0; i < text.length(); i++)
		if (!(text[i] == _T('.') || (text[i] >= _T('0') && text[i] <= _T('9'))))
			return false;
	
	if (!Util::isValidIp4(text))
		return false;
	
	result = std::move(text);
	startPos = posBegin;
	endPos = posEnd;
	return true;
}

bool ChatCtrl::hitText(tstring& text, int selBegin, int selEnd) const
{
	text.resize(selEnd - selBegin);
	GetTextRange(selBegin, selEnd, &text[0]);
	return !text.empty();
}

void ChatCtrl::goToEnd(POINT& scrollPos, bool force)
{
	SCROLLINFO si = { 0 };
	si.cbSize = sizeof(si);
	si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
	GetScrollInfo(SB_VERT, &si);
	if (autoScroll || force)
		PostMessage(EM_SCROLL, SB_BOTTOM, 0);
	SetScrollPos(&scrollPos);
}

void ChatCtrl::goToEnd(bool force)
{
	POINT pt = { 0 };
	GetScrollPos(&pt);
	goToEnd(pt, force);
	if (autoScroll || force)
		PostMessage(EM_SCROLL, SB_BOTTOM, 0);
}

void ChatCtrl::invertAutoScroll()
{
	setAutoScroll(!autoScroll);
}

void ChatCtrl::setAutoScroll(bool flag)
{
	autoScroll = flag;
	if (autoScroll)
		goToEnd(false);
}

LRESULT ChatCtrl::onRButtonDown(POINT pt)
{
	selectedLine = LineFromChar(CharFromPos(pt));

	g_sSelectedText.clear();
	g_sSelectedUserName.clear();
	g_sSelectedIP.clear();
	g_sSelectedURL.clear();
	
	LONG selBegin, selEnd;
	GetSel(selBegin, selEnd);

	if (selEnd > selBegin)
	{
		CHARFORMAT2 cf;
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_LINK;
		GetSelectionCharFormat(cf);
		if (cf.dwEffects & CFE_LINK)
		{
			g_sSelectedURL = getUrl(selBegin, selEnd, true);
			if (!g_sSelectedURL.empty()) return 1;
		}
	}
	
	const int charPos = CharFromPos(pt);
	int begin, end;
	if (selEnd > selBegin && charPos >= selBegin && charPos <= selEnd)
	{
		if (!hitIP(pt, g_sSelectedIP, begin, end))
			if (!hitNick(pt, g_sSelectedUserName, begin, end))
				hitText(g_sSelectedText, selBegin, selEnd);
				
		return 1;
	}
	
	// hightlight IP or nick when clicking on it
	if (hitIP(pt, g_sSelectedIP, begin, end) || hitNick(pt, g_sSelectedUserName, begin, end))
	{
		SetSel(begin, end);
		InvalidateRect(nullptr);
	}
	return 1;
}

//[+] sergiy.karasov
//отключение автоскролла в окне чата при вращении колеса мыши вверх
//влючать автоскролл либо в меню (по правому клику), либо проскроллировав (колесом мыши) до самого низа
LRESULT ChatCtrl::onMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	RECT rc;
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; // location of mouse click
	SCROLLINFO si = { 0 };
	
	// Get the bounding rectangle of the client area.
	ChatCtrl::GetClientRect(&rc);
	ChatCtrl::ScreenToClient(&pt);
	if (PtInRect(&rc, pt))
	{
		si.cbSize = sizeof(si);
		si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
		GetScrollInfo(SB_VERT, &si);
		
		if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) //positive - wheel was rotated forward
		{
			if (si.nMin != si.nPos) //бегунок не в начале или есть скроллбар
			{
				setAutoScroll(false);
			}
		}
		else //negative value indicates that the wheel was rotated backward, toward the user
		{
			//макс достигнут
			//проблемка - мы обрабатываем WM_MOUSEWHEEL перед его основным обработчиком,
			//поэтому автоскролл, зачастую, врубается при дополнительном (т.е. конец уже достигнут, но надо еще раз)
			//вращении колеса мышки вниз
			if (si.nPos == int(si.nMax - si.nPage))
			{
				setAutoScroll(true);
			}
		}
	}
	bHandled = FALSE;
	return 1;
}

tstring ChatCtrl::getUrlHiddenText(LONG end)
{
	TCHAR sep[2] = {};
	GetTextRange(end, end + 1, sep);
	if (sep[0] != HIDDEN_TEXT_SEP) return Util::emptyStringT;
	int line = LineFromChar(end);
	if (line < 0) return Util::emptyStringT;
	int nextStart = line + 1 >= GetLineCount() ? GetTextLength() : LineIndex(line + 1);
	if (end >= nextStart) return Util::emptyStringT;
	int hiddenLen = nextStart - end;
	tstring lineBuf;
	lineBuf.resize(hiddenLen + 1);
	GetTextRange(end, nextStart, &lineBuf[0]);
	auto pos = lineBuf.find(HIDDEN_TEXT_SEP, 1);
	if (pos == tstring::npos) return Util::emptyStringT;
	return lineBuf.substr(1, pos - 1);
}

tstring ChatCtrl::getUrl(LONG start, LONG end, bool keepSelected)
{
	tstring text;
	if (end > start)
	{
		text.resize(end - start + 1);
		SetSel(start, end);
		GetSelText(&text[0]);
		if (!text.empty())
			text.resize(text.length()-1);
		auto pos1 = text.find(HIDDEN_TEXT_SEP);
		if (pos1 != tstring::npos)
		{
			auto pos2 = text.find(HIDDEN_TEXT_SEP, pos1 + 1);
			if (pos2 != tstring::npos)
				text = text.substr(pos1 + 1, pos2 - (pos1 + 1));
		}
		else
		{
			tstring hiddenText = getUrlHiddenText(end);
			if (!hiddenText.empty())
				text = std::move(hiddenText);
		}
		if (!keepSelected)
			SetSel(end, end);
	}
	return text;
}

tstring ChatCtrl::getUrl(const ENLINK* el, bool keepSelected)
{
	return getUrl(el->chrg.cpMin, el->chrg.cpMax, keepSelected);
}

LRESULT ChatCtrl::onEnLink(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	ENLINK* pEL = (ENLINK*)pnmh;
	if (pEL->msg == WM_LBUTTONUP)
	{
		if (!(pEL->chrg.cpMin == ignoreLinkStart && pEL->chrg.cpMax == ignoreLinkEnd))
		{
			g_sSelectedURL = getUrl(pEL, false);
			dcassert(!g_sSelectedURL.empty());
			GetParent().PostMessage(WMU_CHAT_LINK_CLICKED, 0, 0);
		}
		ignoreLinkStart = ignoreLinkEnd = 0;
	}
	else if (pEL->msg == WM_RBUTTONUP)
	{
		g_sSelectedURL = getUrl(pEL, true);
		InvalidateRect(NULL);
		ignoreLinkStart = ignoreLinkEnd = 0;
	}
	else if (pEL->msg == WM_MOUSEMOVE)
	{
		ignoreLinkStart = pEL->chrg.cpMin;
		ignoreLinkEnd = pEL->chrg.cpMax;
	}
	else if (pEL->msg == WM_LBUTTONDOWN)
	{
		ignoreLinkStart = ignoreLinkEnd = 0;
	}
	return 0;
}

IRichEditOle* ChatCtrl::getRichEditOle()
{
	if (!pRichEditOle) initRichEditOle();
	return pRichEditOle;
}

void ChatCtrl::setNeedle(const tstring& needle)
{
	if (needle != currentNeedle)
	{
		currentNeedle = needle;
		findInit = true;
	}
}

bool ChatCtrl::findText()
{
	CHARRANGE savedSel = { -1, 0 };

	FINDTEXTEX ft = {};
	GetSel(ft.chrg);
	dcdebug("findText: selection=(%d, %d)\n", ft.chrg.cpMin, ft.chrg.cpMax);
	if (findInit || ft.chrg.cpMin == ft.chrg.cpMax)
	{
		if (currentFindFlags & FR_DOWN)
		{
			findRangeStart = ft.chrg.cpMin;
			findRangeEnd = -1;
		}
		else
		{
			findRangeStart = 0;
			findRangeEnd = ft.chrg.cpMax;
		}
	}
	else if (ft.chrg.cpMin != ft.chrg.cpMax) 
		savedSel = ft.chrg;

	ft.lpstrText = const_cast<TCHAR*>(currentNeedle.c_str());
	if (ft.chrg.cpMin != ft.chrg.cpMax && !findInit && (currentFindFlags & FR_DOWN))
		ft.chrg.cpMin++;

	findInit = false;
	ft.chrg.cpMax = (currentFindFlags & FR_DOWN) ? findRangeEnd : findRangeStart;
	bool foundHiddenText = false;
	while (true)
	{
		dcdebug("findText: Search in range %d - %d\n", ft.chrg.cpMin, ft.chrg.cpMax);
		long result = findAndSelect(currentFindFlags, ft);
		if (result == -2)
		{
			ft.chrg.cpMin = ft.chrgText.cpMin;
			if (currentFindFlags & FR_DOWN) ft.chrg.cpMin++;
			foundHiddenText = true;
			continue;
		}
		if (result != -1) return true;
		if ((currentFindFlags & FR_DOWN) && findRangeStart > 0)
		{
			dcdebug("findText: wrap around\n");
			findRangeEnd = findRangeStart;
			findRangeStart = 0;
			ft.chrg.cpMin = findRangeStart;
			ft.chrg.cpMax = findRangeEnd;
			result = findAndSelect(currentFindFlags, ft);
		}
		else
		if (!(currentFindFlags & FR_DOWN) && findRangeStart == 0)
		{
			dcdebug("findText: wrap around\n");
			findRangeStart = findRangeEnd;
			findRangeEnd = GetTextLength();
			ft.chrg.cpMin = findRangeEnd;
			ft.chrg.cpMax = findRangeStart;
			result = findAndSelect(currentFindFlags, ft);
		}
		if (result == -2)
		{
			ft.chrg.cpMin = ft.chrgText.cpMin;
			if (currentFindFlags & FR_DOWN) ft.chrg.cpMin++;
			foundHiddenText = true;
			continue;
		}
		if (result != -1) return true;
		break;
	}
	if (foundHiddenText && savedSel.cpMin != -1)
		SetSel(savedSel);
	return false;
}

long ChatCtrl::findAndSelect(DWORD flags, FINDTEXTEX& ft)
{
	long index = FindText(flags, ft);
	dcdebug("findAndSelect: %d, %d, %d\n", index, ft.chrgText.cpMin, ft.chrgText.cpMax);
	if (index != -1)
	{
		SetSel(ft.chrgText);
		CHARRANGE cr;
		GetSel(cr);
		if (ft.chrgText.cpMin != cr.cpMin)
		{
			dcdebug("findAndSelect: hidden text found\n");
			return -2;
		}
	}
	return index;
}

void ChatCtrl::removeHiddenText(tstring& s)
{
	tstring::size_type pos = 0;
	while (pos < s.length())
	{
		auto start = s.find(HIDDEN_TEXT_SEP, pos);
		if (start == tstring::npos) break;
		auto end = s.find(HIDDEN_TEXT_SEP, start + 1);
		if (end == tstring::npos) break;
		s.erase(start, end - start + 1);
		pos = start;
	}
}

#ifdef BL_UI_FEATURE_EMOTICONS
void ChatCtrl::replaceObjects(tstring& s, int startIndex) const
{
	if (!pRichEditOle) return;
	REOBJECT ro;
	tstring::size_type pos = 0;
	int delta = 0;
	for (;;)
	{
		auto nextPos = s.find(WCH_EMBEDDING, pos);
		if (nextPos == tstring::npos) break;
		int shift = 1;
		memset(&ro, 0, sizeof(ro));
		ro.cbStruct = sizeof(ro);
		ro.cp = startIndex + delta + nextPos;
		if (SUCCEEDED(pRichEditOle->GetObject(REO_IOB_USE_CP, &ro, REO_GETOBJ_POLEOBJ)))
		{
			IGDIImage* pImage = nullptr;
			if (SUCCEEDED(ro.poleobj->QueryInterface(IID_IGDIImage, (void**) &pImage)))
			{
				BSTR text;
				if (SUCCEEDED(pImage->get_Text(&text)))
				{
					int textLen = wcslen(text);
					s.replace(nextPos, 1, text, textLen);
					shift = textLen;
					delta += 1 - textLen;
					SysFreeString(text);
				}
				pImage->Release();
			}
			ro.poleobj->Release();
		}
		pos = nextPos + shift;
	}
}
#endif

LRESULT ChatCtrl::onCopyActualLine(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (selectedLine >= 0)
	{
		int startIndex = LineIndex(selectedLine);
		int len = LineLength(startIndex);
		if (len > 0)
		{
			tstring line;
			line.resize(len);
			len = GetLine(selectedLine, &line[0], len);
			if (len > 0)
			{
				line.resize(len);
#ifdef BL_UI_FEATURE_EMOTICONS
				replaceObjects(line, startIndex);
#endif
				removeHiddenText(line);
				WinUtil::setClipboard(line);
			}
		}
	}
	return 0;
}

LRESULT ChatCtrl::onCopyURL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!g_sSelectedURL.empty())
	{
		WinUtil::setClipboard(g_sSelectedURL);
	}
	return 0;
}

LRESULT ChatCtrl::onDumpUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!g_sSelectedIP.empty())
	{
		IpAddress ip;
		if (Util::parseIpAddress(ip, Text::fromT(g_sSelectedIP)))
		{
			string report = ip.type == AF_INET6 ? "IPv6 Info: " : "IPv4 Info: ";
			report += Identity::formatIpString(ip);
			ClientManager::LockInstanceClients l;
			const auto& clients = l.getData();
			auto i = clients.find(hubHint);
			if (i != clients.end())
				i->second->dumpUserInfo(report);
		}
	}
	return 0;
}

LRESULT ChatCtrl::onEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
#if 1
	if (!pRichEditOle)
	{
		initRichEditOle();
		if (!pRichEditOle) return 0;
	}
	LONG start, end;
	GetSel(start, end);
	ITextDocument* pTextDoc = nullptr;
	if (SUCCEEDED(pRichEditOle->QueryInterface(IID_ITextDocument, (void**) &pTextDoc)))
	{
		ITextRange* pRange = nullptr;
		if (SUCCEEDED(pTextDoc->Range(start, end, &pRange)))
		{
			BSTR text;
			if (SUCCEEDED(pRange->GetText(&text)) && text)
			{
				tstring s = text;
				SysFreeString(text);
#ifdef BL_UI_FEATURE_EMOTICONS
				replaceObjects(s, start);
#endif
				removeHiddenText(s);
				WinUtil::setClipboard(s);
			}
			pRange->Release();
		}
		pTextDoc->Release();
	}
#else
	Copy();
#endif
	return 0;
}

LRESULT ChatCtrl::onEditSelectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SetSelAll();
	return 0;
}

LRESULT ChatCtrl::onEditClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Clear();
	return 0;
}

void ChatCtrl::setHubParam(const string& url, const string& nick)
{
	myNick = Text::toT(nick);
	hubHint = url;
}

void ChatCtrl::Clear()
{
	SetWindowText(Util::emptyStringT.c_str());
}

void ChatCtrl::initRichEditOle()
{
	pRichEditOle = GetOleInterface();
	dcassert(pRichEditOle);
#ifdef BL_UI_FEATURE_EMOTICONS
	SetOleCallback(this);
#endif
}

#ifdef BL_UI_FEATURE_EMOTICONS
HRESULT STDMETHODCALLTYPE ChatCtrl::QueryInterface(THIS_ REFIID riid, LPVOID FAR * lplpObj)
{
	HRESULT res = E_NOINTERFACE;
	
	if (riid == IID_IRichEditOleCallback)
	{
		*lplpObj = this;
		res = S_OK;
	}
	
	return res;
}

COM_DECLSPEC_NOTHROW ULONG STDMETHODCALLTYPE ChatCtrl::AddRef(THIS)
{
	return InterlockedIncrement(&refs);
}

COM_DECLSPEC_NOTHROW ULONG STDMETHODCALLTYPE ChatCtrl::Release(THIS)
{
	return InterlockedDecrement(&refs);
}

// *** IRichEditOleCallback methods ***
COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetNewStorage(THIS_ LPSTORAGE FAR * lplpstg)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetInPlaceContext(THIS_ LPOLEINPLACEFRAME FAR * lplpFrame,
                                                                           LPOLEINPLACEUIWINDOW FAR * lplpDoc,
                                                                           LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	if (lplpFrame)
		*lplpFrame = nullptr;
	if (lplpDoc)
		*lplpDoc = nullptr;
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::ShowContainerUI(THIS_ BOOL fShow)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::QueryInsertObject(THIS_ LPCLSID lpclsid, LPSTORAGE lpstg,
                                                                           LONG cp)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::DeleteObject(THIS_ LPOLEOBJECT lpoleobj)
{
	IGDIImageDeleteNotify *pDeleteNotify;
	if (lpoleobj->QueryInterface(IID_IGDIImageDeleteNotify, (void**)&pDeleteNotify) == S_OK && pDeleteNotify)
	{
		pDeleteNotify->SetDelete();
		pDeleteNotify->Release();
	}
	
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::QueryAcceptData(THIS_ LPDATAOBJECT lpdataobj,
                                                                         CLIPFORMAT FAR * lpcfFormat, DWORD reco,
                                                                         BOOL fReally, HGLOBAL hMetaPict)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::ContextSensitiveHelp(THIS_ BOOL fEnterMode)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetClipboardData(THIS_ CHARRANGE FAR * lpchrg, DWORD reco,
                                                                          LPDATAOBJECT FAR * lplpdataobj)
{
	return E_NOTIMPL;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetDragDropEffect(THIS_ BOOL fDrag, DWORD grfKeyState,
                                                                           LPDWORD pdwEffect)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetContextMenu(THIS_ WORD seltype, LPOLEOBJECT lpoleobj,
                                                                        CHARRANGE FAR * lpchrg,
                                                                        HMENU FAR * lphmenu)
{
	// Forward message to host window
	::DefWindowProc(m_hWnd, WM_CONTEXTMENU, (WPARAM)m_hWnd, GetMessagePos());
	return S_OK;
}
#endif // BL_UI_FEATURE_EMOTICONS

#include "stdafx.h"
#include "ChatTextParser.h"
#include "Colors.h"
#include "../client/Text.h"
#include "../client/MagnetLink.h"
#include "../client/FormatUtil.h"
#include "../client/Util.h"

ChatTextParser chatTextParser;

static const tstring badUrlChars(_T("\r\n \"<>[]"));
static const tstring urlDelimChars(_T(",.;!?"));
static const tstring objectReplacement(1, HIDDEN_TEXT_SEP);

static const tstring linkPrefixes[] =
{
	_T("magnet:?"),
	_T("dchub://"),
	_T("nmdc://"),
	_T("nmdcs://"),
	_T("adc://"),
	_T("adcs://"),
	_T("http://"),
	_T("https://"),
	_T("ftp://"),
	_T("irc://"),
	_T("skype:"),
	_T("ed2k://"),
	_T("mms://"),
	_T("xmpp://"),
	_T("nfs://"),
	_T("mailto:"),
	_T("www.")
};

static const int LINK_TYPE_MAGNET = 0;
static const int LINK_TYPE_HTTP   = 6;
static const int LINK_TYPE_WWW    = _countof(linkPrefixes)-1;

#ifdef BL_UI_FEATURE_BB_CODES
static const tstring bbCodes[] =
{
	_T("code"),
	_T("b"),
	_T("i"),
	_T("u"),
	_T("s"),
	_T("img"),
	_T("color"),
	_T("url")
};

static int findBBCode(const tstring& tag)
{
	for (int i = 0; i < _countof(bbCodes); ++i)
		if (bbCodes[i] == tag) return i;
	return -1;
}

static int getLinkType(const tstring& link)
{
	for (int i = 0; i < _countof(linkPrefixes); ++i)
		if (Text::isAsciiPrefix2(link, linkPrefixes[i]))
			return i;
	return -1;
}
#endif

static tstring getMagnetDescription(const tstring& link, const MagnetLink& magnet)
{
	bool isTorrentLink = Util::isTorrentLink(link);
	tstring result = Text::toT(magnet.getFileName());
	tstring details;
	if (magnet.exactLength > 0)
		details = Util::formatBytesT(magnet.exactLength);
	if (magnet.dirSize > 0)
	{
		if (!details.empty()) details += _T(", ");
		details += TSTRING(SETTINGS_SHARE_SIZE) + _T(' ') + Util::formatBytesT(magnet.dirSize);
	}
	if (isTorrentLink)
	{
		if (!details.empty()) details += _T(", ");
		details += TSTRING(BT_LINK);
	}
	if (!isTorrentLink && !magnet.exactSource.empty())
	{
		if (!details.empty()) details += _T(", ");
		details += TSTRING(HUB) + _T(": ") + Text::toT(Util::formatDchubUrl(magnet.exactSource));
	}
	if (!magnet.acceptableSource.empty())
	{
		if (!details.empty()) details += _T(", ");
		details += TSTRING(WEB_URL) + _T(": ") + Text::toT(magnet.acceptableSource);
	}
	if (!details.empty())
	{
		result += _T(" (");
		result += details;
		result += _T(')');
	}
	return result;
}

void ChatTextParser::parseText(const tstring& text, const CHARFORMAT2& cf, bool formatBBCodes, unsigned maxEmoticons)
{
	ASSERT_MAIN_THREAD();
	links.clear();
#ifdef BL_UI_FEATURE_BB_CODES
	tags.clear();
	TagItem ti;
#endif
#ifdef BL_UI_FEATURE_EMOTICONS
	emoticons.clear();
#endif
	LinkItem li;
	li.start = tstring::npos;
	bool openLink = false;
	TCHAR linkPrevChar = 0;
	int ipv6Link = 0;
	StringSetSearch::SearchContext ctx;
	ctx.setIgnoreCase(true);
	tstring::size_type i = 0;
	while (i < text.length())
	{
		if (li.start != tstring::npos)
		{
			TCHAR c = text[i];
			if (c >= _T('A') && c <= _T('Z')) c = c - _T('A') + _T('a');
			if (c == _T('[') && ipv6Link == 0 && i-li.start <= 8 && text[i-1] == _T('/'))
			{
				ipv6Link = 1;
				i++;
				continue;
			}
			if (c == _T(']') && ipv6Link == 1)
			{
				ipv6Link = 2;
				i++;
				continue;
			}
			if (badUrlChars.find(c) == tstring::npos)
			{
				i++;
				continue;
			}
			li.end = i;
			TCHAR pairChar = 0;
			switch (linkPrevChar)
			{
				case _T('('): pairChar = _T(')'); break;
				case _T('{'): pairChar = _T('}'); break;
				case _T('\''): case _T('*'): pairChar = linkPrevChar;
			}
			TCHAR lastChar = text[li.end-1];
			if ((pairChar && lastChar == pairChar) || urlDelimChars.find(lastChar) != tstring::npos)
				--li.end;
			links.push_back(li);
			li.start = tstring::npos;
			li.updatedText.clear();
			ipv6Link = 0;
			i = li.end;
			continue;
		}
		ctx.reset(ss, i);
		uint64_t data;
		if (openLink)
		{
			i = text.find(_T('['), i);
			if (i == tstring::npos) break;
			++i;
			data = TYPE_BBCODE;
		}
		else
		{
			if (!ctx.search(text, ss)) break;
			i = ctx.getCurrentPos();
			data = ctx.getCurrentData();
		}
		int type = data & 3;
#ifdef BL_UI_FEATURE_BB_CODES
		if (type == TYPE_BBCODE && formatBBCodes)
		{
			tstring::size_type tagStart = i - 1;
			bool isClosing;
			int code = processBBCode(text, i, openLink, isClosing);
			if (code != -1)
			{
				if (isClosing)
				{
					int index = tags.size()-1;
					bool urlProcessed = false;
					while (index >= 0)
					{
						if (tags[index].type == code && tags[index].closeTagStart == tstring::npos)
						{
							tags[index].closeTagStart = tagStart;
							tags[index].closeTagEnd = i;
							if (code == BBCODE_URL)
							{
								urlProcessed = true;
								openLink = false;
								break;
							}
						}
						index--;
					}
					if (urlProcessed)
					{
						LinkItem li;
						li.start = tags[index].openTagEnd;
						li.end = tags[index].closeTagStart;
						tstring url, description;
						tags[index].getUrl(text, url, description);
						li.type = getLinkType(url);
						if (li.type == -1 || li.type == LINK_TYPE_WWW)
						{
							if (description.empty()) description = url;
							li.type = LINK_TYPE_HTTP;
							url.insert(0, linkPrefixes[LINK_TYPE_HTTP]);
						}
						else if (li.type == LINK_TYPE_MAGNET && description.empty())
						{
							MagnetLink magnet;
							if (magnet.parse(Text::fromT(url)))
								description = getMagnetDescription(url, magnet);
						}
						if (description.empty()) description = url;
						li.updatedText = std::move(description);
						li.updatedText += HIDDEN_TEXT_SEP;
						li.updatedText += url;
						li.updatedText += HIDDEN_TEXT_SEP;
						li.hiddenTextLen = url.length() + 2;
						links.emplace_back(std::move(li));
					}
				}
				else
				{
					const CHARFORMAT2* prevFmt = getPrevFormat();
					if (!prevFmt) prevFmt = &cf;
					if (processStartTag(ti, text, tagStart, i, *prevFmt))
					{
						tags.push_back(ti);
						if (ti.type == BBCODE_URL)
						{
							dcassert(!openLink);
							openLink = true;
						}
					}
				}
			}
			continue;
		}
#endif
		if (type == TYPE_LINK)
		{
			int index = data >> 2;
			tstring::size_type len = linkPrefixes[index].length();
			dcassert(i >= len);
			tstring::size_type start = i - len;
			if (start == 0 || !_istalpha(text[start-1]))
			{
				li.type = index;
				li.start = start;
				li.end = tstring::npos;
				li.hiddenTextLen = 0;
				linkPrevChar = i == 0 ? 0 : text[i-1];
				i = start;
			}
			continue;
		}
#ifdef BL_UI_FEATURE_EMOTICONS
		if (type == TYPE_EMOTICON && (unsigned) emoticons.size() < maxEmoticons)
		{
			const auto& v = keyToText[(uint32_t) (data >> 2)];
			dcassert(!v.empty());
			tstring::size_type len = v.front()->getText().length();
			dcassert(i >= len);
			for (Emoticon* e : v)
				if (!text.compare(i - len, len, e->getText()))
				{
					emoticons.emplace_back(EmoticonItem{e, i - len, i});
					break;
				}
		}
#endif
	}

	if (li.start != tstring::npos)
	{
		li.end = text.length();
		links.push_back(li);
	}
}

void ChatTextParser::processText(tstring& text)
{
	ASSERT_MAIN_THREAD();
#ifdef BL_UI_FEATURE_BB_CODES
	for (size_t i = 0; i < tags.size(); ++i)
	{
		const TagItem& ti = tags[i];
		if (ti.openTagStart != tstring::npos && ti.openTagEnd != tstring::npos &&
		    ti.closeTagStart != tstring::npos && ti.closeTagEnd != tstring::npos)
		{
			tstring::size_type start = ti.openTagStart;
			tstring::size_type len = ti.openTagEnd - ti.openTagStart;
			applyShift(TYPE_BBCODE, i + 1, start, -(int) len);
			text.erase(start, len);

			start = ti.closeTagStart;
			len = ti.closeTagEnd - ti.closeTagStart;
			applyShift(TYPE_BBCODE, i + 1, start, -(int) len);
			text.erase(start, len);
		}
	}
#endif

	for (size_t i = 0; i < links.size(); ++i)
	{
		LinkItem& li = links[i];
		processLink(text, li);
		if (!li.updatedText.empty())
		{
			tstring::size_type len = li.end - li.start;
			int shift = (int) li.updatedText.length() - (int) len;
			applyShift(TYPE_LINK, i, li.start, shift);
			text.replace(li.start, len, li.updatedText);
		}
	}

#ifdef BL_UI_FEATURE_EMOTICONS
	for (size_t i = 0; i < emoticons.size(); ++i)
	{
		EmoticonItem& ei = emoticons[i];
		tstring::size_type len = ei.emoticon->getText().length();
		int shift = 1 - (int) len;
		applyShift(TYPE_EMOTICON, i, ei.start, shift);
		text.replace(ei.start, len, objectReplacement);
	}
#endif
}

void ChatTextParser::findSubstringAvoidingLinks(tstring::size_type& pos, tstring& text, const tstring& str, size_t& currentLink) const
{
	while (pos < text.length())
	{
		auto nextPos = text.find(str, pos);
		if (nextPos == tstring::npos) break;
		while (currentLink < links.size() && links[currentLink].end <= nextPos)
			++currentLink;
		if (currentLink >= links.size() || nextPos + str.length() - 1 < links[currentLink].start)
		{
			pos = nextPos;
			return;
		}
		pos = links[currentLink].end;
	}
	pos = tstring::npos;
}

#ifdef BL_UI_FEATURE_BB_CODES
int ChatTextParser::processBBCode(const tstring& text, tstring::size_type& pos, bool openLink, bool& isClosing)
{
	isClosing = false;
	if (pos + 2 >= text.length()) return -1;
	if (text[pos] == _T('/'))
	{
		isClosing = true;
		pos++;
	}
	tstring::size_type endPos = text.find_first_of(_T(" ]="), pos);
	if (endPos == tstring::npos || endPos - pos > 8) return -1;
	if (openLink && !isClosing) return -1;
	TCHAR endChar = text[endPos];
	if (endChar == _T(' ') || (isClosing && endChar == _T('='))) return -1;
	tstring tag = text.substr(pos, endPos - pos);
	Text::asciiMakeLower(tag);
	int code = findBBCode(tag);
	if (code == -1 || (code != BBCODE_URL && code != BBCODE_COLOR && endChar != _T(']')) || (openLink && code != BBCODE_URL))
		return -1;
	if (endChar == _T('='))
	{
		endPos = text.find(_T(']'), endPos + 1);
		if (endPos == tstring::npos) return -1;
	}
	pos = endPos + 1;
	return code;
}

bool ChatTextParser::processStartTag(ChatTextParser::TagItem& ti, const tstring& text, tstring::size_type start, tstring::size_type end, const CHARFORMAT2& prevFmt)
{
	tstring tag = text.substr(start + 1, end - start - 2);
	Text::asciiMakeLower(tag);
	if (tag.compare(0, 6, _T("color=")) == 0)
	{
		ti.type = BBCODE_COLOR;
		ti.fmt = prevFmt;
		ti.fmt.dwMask |= CFM_COLOR;
		tag.erase(0, 6);
		Colors::getColorFromString(tag, ti.fmt.crTextColor);
		ti.openTagStart = start;
		ti.openTagEnd = end;
		ti.closeTagStart = ti.closeTagEnd = tstring::npos;
		return true;
	}
	if (tag.compare(0, 3, _T("url")) == 0)
	{
		if (tag.length() == 3 || (tag.length() >= 4 && tag[3] == _T('=')))
		{
			ti.type = BBCODE_URL;
			ti.openTagStart = start;
			ti.openTagEnd = end;
			ti.closeTagStart = ti.closeTagEnd = tstring::npos;
			return true;
		}
		return false;
	}
	ti.type = findBBCode(tag);
	switch (ti.type)
	{
		case BBCODE_BOLD:
			ti.fmt = prevFmt;
			ti.fmt.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT;
			ti.fmt.dwEffects |= CFE_BOLD;
			break;
		case BBCODE_ITALIC:
			ti.fmt = prevFmt;
			ti.fmt.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT;
			ti.fmt.dwEffects |= CFE_ITALIC;
			break;
		case BBCODE_UNDERLINE:
			ti.fmt = prevFmt;
			ti.fmt.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT;
			ti.fmt.dwEffects |= CFE_UNDERLINE;
			break;
		case BBCODE_STRIKEOUT:
			ti.fmt = prevFmt;
			ti.fmt.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT;
			ti.fmt.dwEffects |= CFE_STRIKEOUT;
			break;
		default:
			return false;
	}
	ti.openTagStart = start;
	ti.openTagEnd = end;
	ti.closeTagStart = ti.closeTagEnd = tstring::npos;
	return true;
}

const CHARFORMAT2* ChatTextParser::getPrevFormat() const
{
	for (auto i = tags.rbegin(); i != tags.rend(); ++i)
	{
		const TagItem& ti = *i;
		if (ti.closeTagStart == tstring::npos) return &ti.fmt;
	}
	return nullptr;
}
#endif

void ChatTextParser::processLink(const tstring& text, ChatTextParser::LinkItem& li)
{
	if (!li.updatedText.empty()) return;
	if (li.type == LINK_TYPE_MAGNET)
	{
		tstring link = text.substr(li.start, li.end - li.start);
		MagnetLink magnet;
		if (!magnet.parse(Text::fromT(link)))
		{
			li.start = li.end = tstring::npos;
			li.type = -1;
			li.updatedText.clear();
			return;
		}
		li.updatedText = getMagnetDescription(link, magnet);
		li.updatedText += HIDDEN_TEXT_SEP;
		li.updatedText += link;
		li.updatedText += HIDDEN_TEXT_SEP;
		li.hiddenTextLen = link.length() + 2;
	}
	else if (li.type == LINK_TYPE_WWW)
	{
		tstring link = text.substr(li.start, li.end - li.start);
		li.updatedText = link;
		link.insert(0, linkPrefixes[LINK_TYPE_HTTP]);
		li.updatedText += HIDDEN_TEXT_SEP;
		li.updatedText += link;
		li.updatedText += HIDDEN_TEXT_SEP;
		li.hiddenTextLen = link.length() + 2;
	}
}

static inline void shiftPos(tstring::size_type& pos, tstring::size_type movePos, int shift)
{
	if (pos != tstring::npos && pos > movePos)
		pos += shift;
}

void ChatTextParser::applyShift(int what, size_t startIndex, tstring::size_type start, int shift)
{
#ifdef BL_UI_FEATURE_BB_CODES
	for (size_t j = what == TYPE_BBCODE ? startIndex : 0; j < tags.size(); ++j)
	{
		shiftPos(tags[j].openTagStart, start, shift);
		shiftPos(tags[j].openTagEnd, start, shift);
	}
	for (size_t j = 0; j < tags.size(); ++j)
	{
		shiftPos(tags[j].closeTagStart, start, shift);
		shiftPos(tags[j].closeTagEnd, start, shift);
	}
#endif
	for (size_t j = what == TYPE_LINK ? startIndex : 0; j < links.size(); ++j)
	{
		shiftPos(links[j].start, start, shift);
		shiftPos(links[j].end, start, shift);
	}
#ifdef BL_UI_FEATURE_EMOTICONS
	for (size_t j = what == TYPE_EMOTICON ? startIndex : 0; j < emoticons.size(); ++j)
	{
		shiftPos(emoticons[j].start, start, shift);
		shiftPos(emoticons[j].end, start, shift);
	}
#endif
}

void ChatTextParser::clear()
{
#ifdef BL_UI_FEATURE_BB_CODES
	tags.clear();
#endif
	links.clear();
#ifdef BL_UI_FEATURE_EMOTICONS
	emoticons.clear();
#endif
}

#ifdef BL_UI_FEATURE_BB_CODES
void ChatTextParser::TagItem::getUrl(const tstring& text, tstring& url, tstring& description) const
{
	if (type != BBCODE_URL) return;
	description = text.substr(openTagEnd, closeTagStart - openTagEnd);
	if (openTagEnd - openTagStart > 6)
		url = text.substr(openTagStart + 5, openTagEnd - 1 - (openTagStart + 5));
	else
	{
		url = std::move(description);
		description.clear();
	}
}
#endif

void ChatTextParser::initSearch()
{
	ss.clear();
#ifdef BL_UI_FEATURE_BB_CODES
	ss.addString(_T("["), TYPE_BBCODE);
#endif
	for (int i = 0; i < _countof(linkPrefixes); ++i)
		ss.addString(linkPrefixes[i], TYPE_LINK | i << 2);
#ifdef BL_UI_FEATURE_EMOTICONS
	textToKey.clear();
	keyToText.clear();
	nextKey = 0;
	if (emtConfig)
	{
		for (const auto& pack : emtConfig->getPacks())
			for (Emoticon* e : pack->getSortedEmoticons())
			{
				tstring text = Text::toLower(e->getText());
				uint32_t key = addKey(text, e);
				if (key)
					ss.addString(text, (uint64_t) key << 2 | TYPE_EMOTICON);
			}
	}
#endif
}

#ifdef BL_UI_FEATURE_EMOTICONS
uint32_t ChatTextParser::addKey(const tstring& text, Emoticon* emoticon)
{
	uint32_t key;
	auto i = textToKey.find(text);
	if (i == textToKey.end())
	{
		key = ++nextKey;
		textToKey.insert(make_pair(text, key));
	}
	else
		key = i->second;
	auto& v = keyToText[key];
	for (const Emoticon* e : v)
		if (e->getText() == emoticon->getText())
			return 0;
	dcassert(v.empty() || v.back()->getText().length() == emoticon->getText().length());
	v.push_back(emoticon);
	return key;
}
#endif

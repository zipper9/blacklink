#include "stdafx.h"
#include "ChatTextParser.h"
#include "Colors.h"
#include "../client/Text.h"
#include "../client/MagnetLink.h"
#include "../client/Util.h"

static const tstring badUrlChars(_T("\r\n \"<>[]"));
static const tstring urlDelimChars(_T(",.;!?"));

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

struct SubstringInfo
{
	uint64_t value;
	uint64_t mask;
};

static SubstringInfo substringInfo[_countof(linkPrefixes)];

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

static void makeSubstringInfo()
{
	for (int i = 0; i < _countof(linkPrefixes); ++i)
	{
		uint64_t mask = std::numeric_limits<uint64_t>::max();
		uint64_t val = 0;
		const TCHAR* c = linkPrefixes[i].data();
		size_t len = linkPrefixes[i].length();
		if (len > 8)
		{
			c += len - 8;
			len = 8;
		}
		else
			mask >>= (8 - len) * 8;
		for (size_t i = 0; i < len; ++i)
			val = val<<8 | (uint8_t) c[i];
		substringInfo[i].mask = mask;
		substringInfo[i].value = val;
	}
}

void ChatTextParser::parseText(const tstring& text, const CHARFORMAT2& cf, bool formatBBCodes)
{
	static std::atomic_bool substringInfoInitialized = false;
	if (!substringInfoInitialized)
	{
		makeSubstringInfo();
		substringInfoInitialized = true;
	}

	links.clear();
	uint64_t hash = 0;
	bool inUrl = false;
#ifdef BL_UI_FEATURE_BB_CODES
	tags.clear();
	tstring tagData;
	tstring::size_type tagStart = tstring::npos;
	TagItem ti;
#endif
	LinkItem li;
	li.start = tstring::npos;
	TCHAR linkPrevChar = 0;
	int ipv6Link = 0;
	for (tstring::size_type i = 0; i < text.length(); ++i)
	{
		TCHAR c = text[i];
		if (c >= _T('A') && c <= _T('Z')) c = c - _T('A') + _T('a');
		hash = hash << 8 | (uint8_t) c;
		if (li.start != tstring::npos)
		{
			if (c == _T('[') && ipv6Link == 0 && i-li.start <= 8 && text[i-1] == _T('/'))
			{
				ipv6Link = 1;
				continue;
			}
			if (c == _T(']') && ipv6Link == 1)
			{
				ipv6Link = 2;
				continue;
			}
			if (badUrlChars.find(c) == tstring::npos)
				continue;
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
			ipv6Link = 0;
		}
#ifdef BL_UI_FEATURE_BB_CODES
		if (c == _T('[') && formatBBCodes)
		{
			tagStart = i;
			tagData.clear();
			continue;
		}
		if (c == _T(']'))
		{
			if (!tagData.empty())
			{
				if (tagData[0] == _T('/'))
				{
					tagData.erase(0, 1);
					int type = findBBCode(tagData);
					if (type == BBCODE_URL) inUrl = false;
					if (type != -1)
					{
						int index = tags.size()-1;
						bool urlProcessed = false;
						while (index >= 0)
						{
							if (tags[index].type == type && tags[index].closeTagStart == tstring::npos)
							{
								tags[index].closeTagStart = tagStart;
								tags[index].closeTagEnd = i + 1;
								if (type == BBCODE_URL)
								{
									urlProcessed = true;
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
							tstring url;
							tags[index].getUrl(text, url, li.updatedText);
							if (li.updatedText.empty()) li.updatedText = url;
							li.type = getLinkType(url);
							if (li.type == -1 || li.type == LINK_TYPE_WWW)
							{
								li.type = LINK_TYPE_HTTP;
								url.insert(0, linkPrefixes[LINK_TYPE_HTTP]);
							}
							li.updatedText += HIDDEN_TEXT_SEP;
							li.updatedText += url;
							li.updatedText += HIDDEN_TEXT_SEP;
							li.hiddenTextLen = url.length() + 2;
							links.emplace_back(std::move(li));
						}
					}
					tagStart = tstring::npos;
					tagData.clear();
					continue;
				}
				const CHARFORMAT2* prevFmt = getPrevFormat();
				if (!prevFmt) prevFmt = &cf;
				if (processTag(ti, tagData, tagStart, i + 1, *prevFmt))
					tags.push_back(ti);
			}
			continue;
		}
		if (tagStart != tstring::npos && (tagData.length() < 32 || inUrl))
		{
			tagData += c;
			if (tagData.length() == 4 && tagData == _T("url=")) inUrl = true;
		}
#endif
		if (!inUrl)
			for (int j = 0; j < _countof(linkPrefixes); ++j)
				if ((hash & substringInfo[j].mask) == substringInfo[j].value)
				{
					const tstring& link = linkPrefixes[j];
					tstring::size_type start = i+1-link.length();
					if (i >= link.length()-1 && text.compare(start, link.length(), link) == 0 &&
					    (start == 0 || !_istalpha(text[start-1])))
					{
						li.type = j;
						li.start = start;
						li.end = tstring::npos;
						li.hiddenTextLen = 0;
						linkPrevChar = start == 0 ? 0 : text[start-1];
					}
					break;
				}
	}

	if (li.start != tstring::npos)
	{
		li.end = text.length();
		links.push_back(li);
	}
}

void ChatTextParser::processText(tstring& text)
{
#ifdef BL_UI_FEATURE_BB_CODES
	for (size_t i = 0; i < tags.size(); ++i)
	{
		const TagItem& ti = tags[i];
		if (ti.openTagStart != tstring::npos && ti.openTagEnd != tstring::npos &&
		    ti.closeTagStart != tstring::npos && ti.closeTagEnd != tstring::npos)
		{
			tstring::size_type start = ti.openTagStart;
			tstring::size_type len = ti.openTagEnd - ti.openTagStart;
			applyShift(i + 1, 0, start, -(int) len);
			text.erase(start, len);

			start = ti.closeTagStart;
			len = ti.closeTagEnd - ti.closeTagStart;
			applyShift(i + 1, 0, start, -(int) len);
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
			applyShift(0, i, li.start, shift);
			text.replace(li.start, len, li.updatedText);
		}
	}
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
bool ChatTextParser::processTag(ChatTextParser::TagItem& ti, tstring& tag, tstring::size_type start, tstring::size_type end, const CHARFORMAT2& prevFmt)
{
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
	for (auto i = tags.rbegin(); i != tags.rend(); i++)
	{
		const TagItem& ti = *i;
		if (ti.closeTagStart == tstring::npos) return &ti.fmt;
	}
	return nullptr;
}
#endif

void ChatTextParser::processLink(const tstring& text, ChatTextParser::LinkItem& li)
{
	if (li.type == LINK_TYPE_MAGNET)
	{
		tstring link = text.substr(li.start, li.end - li.start);
		MagnetLink magnet;
		if (!magnet.parse(Text::fromT(link)))
		{
			li.start = li.end = tstring::npos;
			li.type = -1;
			return;
		}
		bool isTorrentLink = Util::isTorrentLink(link);
		li.updatedText = Text::toT(magnet.getFileName());
		tstring description;
		if (magnet.exactLength > 0)
			description = Util::formatBytesT(magnet.exactLength);
		if (magnet.dirSize > 0)
		{
			if (!description.empty()) description += _T(", ");
			description += TSTRING(SETTINGS_SHARE_SIZE) + _T(' ') + Util::formatBytesT(magnet.dirSize);
		}
		if (isTorrentLink)
		{
			if (!description.empty()) description += _T(", ");
			description += TSTRING(BT_LINK);
		}
		if (!isTorrentLink && !magnet.exactSource.empty())
		{
			if (!description.empty()) description += _T(", ");
			description += TSTRING(HUB) + _T(": ") + Text::toT(Util::formatDchubUrl(magnet.exactSource));
		}
		if (!magnet.acceptableSource.empty())
		{
			if (!description.empty()) description += _T(", ");
			description += TSTRING(WEB_URL) + _T(": ") + Text::toT(magnet.acceptableSource);
		}
		if (!description.empty())
		{
			li.updatedText += _T(" (");
			li.updatedText += description;
			li.updatedText += _T(')');
		}
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

void ChatTextParser::applyShift(size_t tagsStartIndex, size_t linksStartIndex, tstring::size_type start, int shift)
{
#ifdef BL_UI_FEATURE_BB_CODES
	for (size_t j = tagsStartIndex; j < tags.size(); ++j)
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
	for (size_t j = linksStartIndex; j < links.size(); ++j)
	{
		shiftPos(links[j].start, start, shift);
		shiftPos(links[j].end, start, shift);
	}
}

void ChatTextParser::clear()
{
#ifdef BL_UI_FEATURE_BB_CODES
	tags.clear();
#endif
	links.clear();
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

#ifndef CHAT_TEXT_PARSER_H_
#define CHAT_TEXT_PARSER_H_

#include "StringSetSearch.h"
#include "Emoticons.h"

#ifdef _UNICODE
#define HIDDEN_TEXT_SEP L'\x241F'
#else
#define HIDDEN_TEXT_SEP '\x05'
#endif

class ChatTextParser
{
	public:
		enum
		{
			TYPE_LINK = 1,
			TYPE_BBCODE,
			TYPE_EMOTICON
		};

#ifdef BL_UI_FEATURE_BB_CODES
		enum
		{
			BBCODE_CODE,
			BBCODE_BOLD,
			BBCODE_ITALIC,
			BBCODE_UNDERLINE,
			BBCODE_STRIKEOUT,
			BBCODE_IMAGE,
			BBCODE_COLOR,
			BBCODE_URL
		};

		struct TagItem
		{
			int type;
			tstring::size_type openTagStart;
			tstring::size_type openTagEnd;
			tstring::size_type closeTagStart;
			tstring::size_type closeTagEnd;
			CHARFORMAT2 fmt;

			void getUrl(const tstring& text, tstring& url, tstring& description) const;
		};
#endif

		struct EmoticonItem
		{
			Emoticon* emoticon;
			tstring::size_type start;
			tstring::size_type end;
		};

		struct LinkItem
		{
			int type;
			tstring::size_type start;
			tstring::size_type end;
			tstring updatedText;
			int hiddenTextLen;
		};

		void parseText(const tstring& text, const CHARFORMAT2& cf, bool formatBBCodes, unsigned maxEmoticons);
		void processText(tstring& text);
		void findSubstringAvoidingLinks(tstring::size_type& pos, tstring& text, const tstring& str, size_t& currentLink) const;
		void initSearch();
#ifdef BL_UI_FEATURE_EMOTICONS
		void setEmoticonPackList(EmoticonPackList* packList) { emtConfig = packList; }
#endif
		void clear();
#ifdef BL_UI_FEATURE_BB_CODES
		const vector<TagItem>& getTags() const { return tags; }
		vector<TagItem>& getTags() { return tags; }
#endif
		const vector<LinkItem>& getLinks() const { return links; }
#ifdef BL_UI_FEATURE_EMOTICONS
		const vector<EmoticonItem>& getEmoticons() const { return emoticons; }
#endif

	private:
		StringSetSearch::Set ss;
#ifdef BL_UI_FEATURE_BB_CODES
		vector<TagItem> tags;
#endif
#ifdef BL_UI_FEATURE_EMOTICONS
		EmoticonPackList* emtConfig = nullptr;
		vector<EmoticonItem> emoticons;
		boost::unordered_map<tstring, uint32_t> textToKey;
		boost::unordered_map<uint32_t, vector<Emoticon*>> keyToText;
		uint32_t nextKey = 0;
#endif
		vector<LinkItem> links;

		static void processLink(const tstring& text, LinkItem& li);
#ifdef BL_UI_FEATURE_BB_CODES
		static int processBBCode(const tstring& text, tstring::size_type& pos, bool& isClosing);
		const CHARFORMAT2* getPrevFormat() const;
		static bool processStartTag(TagItem& item, const tstring& text, tstring::size_type start, tstring::size_type end, const CHARFORMAT2& prevFmt);
#endif
#ifdef BL_UI_FEATURE_EMOTICONS
		uint32_t addKey(const tstring& text, Emoticon* emoticon);
#endif
		void applyShift(int what, size_t startIndex, tstring::size_type start, int shift);
};

extern ChatTextParser chatTextParser;

#endif // CHAT_TEXT_PARSER_H_

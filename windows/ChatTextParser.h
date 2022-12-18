#ifndef CHAT_TEXT_PARSER_H_
#define CHAT_TEXT_PARSER_H_

#include "../client/w.h"
#include "../client/typedefs.h"

#ifdef _UNICODE
#define HIDDEN_TEXT_SEP L'\x241F'
#else
#define HIDDEN_TEXT_SEP '\x05'
#endif

class ChatTextParser
{
	public:
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

		struct LinkItem
		{
			int type;
			tstring::size_type start;
			tstring::size_type end;
			tstring updatedText;
			int hiddenTextLen;
		};

		void parseText(const tstring& text, const CHARFORMAT2& cf, bool formatBBCodes);
		void processText(tstring& text);
		void findSubstringAvoidingLinks(tstring::size_type& pos, tstring& text, const tstring& str, size_t& currentLink) const;
		void applyShift(size_t tagsStartIndex, size_t linksStartIndex, tstring::size_type start, int shift);
		void clear();
#ifdef BL_UI_FEATURE_BB_CODES
		const vector<TagItem>& getTags() const { return tags; }
		vector<TagItem>& getTags() { return tags; }
#endif
		const vector<LinkItem>& getLinks() const { return links; }

	private:
#ifdef BL_UI_FEATURE_BB_CODES
		vector<TagItem> tags;
#endif

		vector<LinkItem> links;

		static void processLink(const tstring& text, LinkItem& li);
#ifdef BL_UI_FEATURE_BB_CODES
		const CHARFORMAT2* getPrevFormat() const;
		static bool processTag(TagItem& item, tstring& tag, tstring::size_type start, tstring::size_type end, const CHARFORMAT2& prevFmt);
#endif
};

#endif // CHAT_TEXT_PARSER_H_

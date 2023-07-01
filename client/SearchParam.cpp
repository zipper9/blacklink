#include "stdinc.h"
#include "SearchParam.h"
#include "SearchResult.h"
#include "Random.h"
#include "Util.h"
#include "SimpleStringTokenizer.h"

SearchTokenList SearchTokenList::instance;

static void normalizeWhitespace(string& s)
{
	for (string::size_type i = 0; i < s.length(); i++)
		if (s[i] == '\t' || s[i] == '\n' || s[i] == '\r')
			s[i] = ' ';
}

void SearchParam::removeToken()
{
	if (token)
	{
		SearchTokenList::instance.removeToken(token);
		token = 0;
	}
}

void SearchParam::generateToken(bool autoToken)
{
	dcassert(!token);
	while (true)
	{
		token = Util::rand();
		if (autoToken)
		{
			token &= ~1;
			break;
		}
		token |= 1;
		if (SearchTokenList::instance.addToken(token, owner)) break;
	}
}

void SearchParam::setFilter(const string& s, int fileType)
{
	filter.clear();
	filterExclude.clear();
	SimpleStringTokenizer<char> st(s, ' ');
	string token;
	while (st.getNextNonEmptyToken(token))
	{
		if (fileType == FILE_TYPE_TTH && (!Util::isTigerHashString(token) || !filter.empty()))
			fileType = FILE_TYPE_ANY;
		if (token[0] != '-')
		{
			if (!filter.empty()) filter += ' ';
			filter += token;
		}
		else if (token.length() > 1)
		{
			if (!filterExclude.empty()) filterExclude += ' ';
			filterExclude += token.substr(1);
		}
	}
	normalizeWhitespace(filter);
	normalizeWhitespace(filterExclude);
	this->fileType = fileType;
}

void SearchParam::prepareFilter()
{
	if (!filter.empty() && fileType != FILE_TYPE_TTH) Text::makeLower(filter);
	if (!filterExclude.empty()) Text::makeLower(filterExclude);
}

bool SearchParam::matchSearchResult(const SearchResult& sr, bool onlyFreeSlots) const
{
	if (onlyFreeSlots && sr.freeSlots < 1) return false;
	if (fileType == FILE_TYPE_TTH)
	{
		if (filter.empty()) return false;
		if (sr.getType() != SearchResult::TYPE_FILE || TTHValue(filter) != sr.getTTH()) return false;
	}
	else
	{
		if (sr.getType() != SearchResult::TYPE_DIRECTORY)
		{
			if (fileType == FILE_TYPE_DIRECTORY) return false;
			else if (fileType > FILE_TYPE_ANY && fileType < NUMBER_OF_FILE_TYPES)
			{
				unsigned extMask = getFileTypesFromFileName(Util::getFileName(sr.getFile()));
				if (!(extMask & 1<<fileType)) return false;
			}
		}
		string fileName = sr.getFile();
		Text::makeLower(fileName);
		string token;
		SimpleStringTokenizer<char> st1(filter, ' ');
		while (st1.getNextNonEmptyToken(token))
		{
			if (fileName.find(token) == string::npos) return false;
		}
		SimpleStringTokenizer<char> st2(filterExclude, ' ');
		while (st2.getNextNonEmptyToken(token))
		{
			if (fileName.find(token) != string::npos) return false;
		}
	}
	if (sr.getType() != SearchResult::TYPE_DIRECTORY && sizeMode != SIZE_DONTCARE)
	{
		switch (sizeMode)
		{
			case SIZE_ATLEAST:
				if (sr.getSize() < size) return false;
				break;
			case SIZE_ATMOST:
				if (sr.getSize() > size) return false;
				break;
			case SIZE_EXACT:
				if (sr.getSize() != size) return false;
		}
	}
	return true;
}

uint64_t SearchTokenList::getTokenOwner(uint32_t token) const
{
	LOCK(tokensLock);
	auto i = tokens.find(token);
	return i == tokens.end() ? 0 : i->second;
}

bool SearchTokenList::addToken(uint32_t token, uint64_t owner)
{
	LOCK(tokensLock);
	return tokens.insert(make_pair(token, owner)).second;
}

void SearchTokenList::removeToken(uint32_t token)
{
	LOCK(tokensLock);
	tokens.erase(token);
}

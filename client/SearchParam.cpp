#include "stdinc.h"
#include "SearchParam.h"
#include "Random.h"

SearchTokenList SearchTokenList::instance;

void SearchParamBase::normalizeWhitespace(string& s)
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

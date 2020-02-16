#ifndef WILDCARDS_H
#define WILDCARDS_H

#include "SimpleStringTokenizer.h"
#include <regex>

// Supported wild-characters: '*', '?'; sets: [a-z], '!' negation
// Examples:
//       '[a-g]l*i?n' matches 'florian'
//       '[!abc]*e' matches 'smile'
//       '[-z] matches 'a'

namespace Wildcards
{
	template<typename char_type>
	bool regexFromPatternList(std::basic_regex<char_type>& re, const std::basic_string<char_type>& s, bool ignoreCase) noexcept
	{
		re = std::basic_regex<char_type>();
		if (s.empty()) return false;
		std::basic_string<char_type> reStr;
		SimpleStringTokenizer<char_type> t(s, ';');
		std::basic_string<char_type> str;
		while (t.getNextNonEmptyToken(str))
		{
			if (!reStr.empty()) reStr += (char_type) '|';
			for (std::basic_string<char_type>::size_type j = 0; j < str.length(); j++)
			{
				if (str[j] == '*')
				{
					reStr += (char_type) '.';
					reStr += (char_type) '*';
					continue;
				}
				if (str[j] == '?')
				{
					reStr += (char_type) '.';
					continue;
				}
				if (str[j] == '[')
				{
					std::basic_string<char_type>::size_type jnext = str.find((char_type) ']', j);
					if (jnext == std::basic_string<char_type>::npos)
					{
						reStr += (char_type) '\\';
						reStr += (char_type) '[';
						continue;
					}
					reStr += (char_type) '[';
					j++;
					if (str[j] == '!')
					{
						if (++j == jnext) return false;
						reStr += (char_type) '^';
					}
					if (str[j] == '-')
					{
						if (++j == jnext) return false;
						reStr += (char_type) '\\';
						reStr += (char_type) 'c';
						reStr += (char_type) 'a';
						reStr += (char_type) '-';
					}
					reStr += str.substr(j, jnext - j + 1);
					j = jnext;
					continue;
				}
				if (str[j] == ']' || str[j] == '^' || str[j] == '$' || str[j] == '\\' ||
				    str[j] == '.' || str[j] == '+' || str[j] == '(' || str[j] == ')'  ||
					str[j] == '{' || str[j] == '}' || str[j] == '|')
				{
					reStr += (char_type) '\\';
				}
				reStr += str[j];
			}
		}
		try
		{
			re.assign(reStr, std::regex_constants::ECMAScript | (ignoreCase ? std::regex_constants::icase : std::regex::flag_type()));
		}
		catch (...)
		{
			re = std::basic_regex<char_type>();
			return false;
		}
		return true;
	}
};

#endif

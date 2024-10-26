#ifndef NO_CASE_HASH_H_
#define NO_CASE_HASH_H_

#include "Text.h"

struct NoCaseStringHash
{
	size_t operator()(const string& s) const
	{
		size_t x = 0;
		const char* d = s.data();
		size_t i = 0, len = s.length();
		while (i < len)
		{
			uint32_t c;
			int n = Text::utf8ToWc(d, i, len, c);
			if (n < 0)
			{
				x = x * 31 + (unsigned char) d[i];
				i -= n;
			}
			else
			{
				if (c < 0x10000) c = Text::toLower(c);
				x = x * 31 + (size_t) c;
				i += n;
			}
		}
		return x;
	}

	size_t operator()(const wstring& s) const
	{
		size_t x = 0;
		const wchar_t* d = s.data();
		size_t len = s.length();
		for (size_t i = 0; i < len; ++i)
			x = x * 31 + (size_t) Text::toLower(d[i]);
		return x;
	}
};

struct NoCaseStringEq
{
	bool operator()(const string& a, const string& b) const
	{
		const char* d1 = a.data();
		const char* d2 = b.data();
		size_t i = 0, len1 = a.length();
		size_t j = 0, len2 = b.length();
		while (i < len1 && j < len2)
		{
			uint32_t c1, c2;
			int n = Text::utf8ToWc(d1, i, len1, c1);
			if (n < 0)
			{
				c1 = (unsigned char) d1[i];
				i -= n;
			}
			else
			{
				if (c1 < 0x10000) c1 = Text::toLower(c1);
				i += n;
			}
			n = Text::utf8ToWc(d2, j, len2, c2);
			if (n < 0)
			{
				c2 = (unsigned char) d2[j];
				j -= n;
			}
			else
			{
				if (c2 < 0x10000) c2 = Text::toLower(c2);
				j += n;
			}
			if (c1 != c2) return false;
		}
		return i == len1 && j == len2;
	}

	bool operator()(const wstring& a, const wstring& b) const
	{
		if (a.length() != b.length()) return false;
		const wchar_t* d1 = a.data();
		const wchar_t* d2 = b.data();
		size_t len = a.length();
		for (size_t i = 0; i < len; ++i)
			if (Text::toLower(d1[i]) != Text::toLower(d2[i])) return false;
		return true;
	}
};

#endif // NO_CASE_HASH_H_

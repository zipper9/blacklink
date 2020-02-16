#ifndef SIMPLE_STRING_TOKENIZER_H_
#define SIMPLE_STRING_TOKENIZER_H_

#include <string>

template<typename char_type>
class SimpleStringTokenizer
{
		using string_type = std::basic_string<char_type>;

	public:
		SimpleStringTokenizer(const string_type& str, char_type c, size_t start = 0) : str(str), c(c), start(start) {}

		bool getNextToken(string_type& res)
		{
			if (start >= str.length())
			{
				res.clear();
				return false;
			}
			string_type::size_type next = str.find(c, start);
			if (next == string_type::npos) next = str.length();
			res.assign(str, start, next-start);
			start = next + 1;
			return true;
		}

		bool getNextNonEmptyToken(string_type& res)
		{
			while (start < str.length())
			{
				if (str[start] == c)
				{
					start++;
					continue;
				}
				string_type::size_type next = str.find(c, start);
				if (next == string_type::npos) next = str.length();
				res.assign(str, start, next-start);
				start = next + 1;
				return true;
			}
			return false;
		}

		void reset(size_t pos = 0)
		{
			start = pos;
		}
	
	private:
		const string_type& str;
		char_type c;
		size_t start;
};

#endif // SIMPLE_STRING_TOKENIZER_H_

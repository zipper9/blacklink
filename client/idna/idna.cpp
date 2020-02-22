#include "stdinc.h"
#include "idna.h"
#include "punycode.h"

static const size_t MAX_NAME_LEN  = 255;
static const size_t MAX_LABEL_LEN = 63;

bool IDNA_convert_to_ACE(const wstring& input, string& output, int& error)
{
	output.clear();
	if (input.empty())
	{
		error = IDNA_INVALID_NAME_SIZE;
		return false;
	}

	char label[MAX_LABEL_LEN];
	wstring::size_type inputLen = input.length();
	bool finalDot = false;
	if (input[inputLen] == L'.')
	{
		inputLen--;
		finalDot = true;
	}

	wstring::size_type i = 0;
	while (i < inputLen)
	{
		bool appendDot = true;
		wstring::size_type pos = input.find(L'.', i);
		if (pos == wstring::npos)
		{
			pos = inputLen;
			appendDot = finalDot;
		}
		size_t labelLen = pos - i;
		if (!labelLen || labelLen > MAX_LABEL_LEN)
		{
			error = IDNA_INVALID_LABEL_SIZE;
			return false;
		}

		bool convert = false;
		for (size_t j = i; j < pos; ++j)
		{
			if ((uint32_t) input[j] <= ' ' || ((uint32_t) input[j] & 0xF800) == 0xD800)
			{
				error = IDNA_BAD_CHARACTER;
				return false;
			}
			if ((uint32_t) input[j] >= 128)
				convert = true;
		}

		if (convert)
		{
			size_t outputLen = MAX_LABEL_LEN - 4;
			punycode_status status = punycode_encode(labelLen, input.c_str() + i, outputLen, label);
			if (status != punycode_success)
			{
				error = status == punycode_big_output ? IDNA_INVALID_LABEL_SIZE : IDNA_CONVERSION_ERROR;
				return false;
			}
			output.append("xn--", 4);
			output.append(label, outputLen);
		}
		else
		{
			for (size_t j = i; j < pos; ++j)
				output += (char) input[j];
		}
		if (appendDot)
			output += '.';
		if (output.length() > MAX_NAME_LEN)
		{
			error = IDNA_INVALID_NAME_SIZE;
			return false;
		}
		i = pos + 1;
	}
	error = 0;
	return true;
}

bool IDNA_convert_from_ACE(const std::string& input, std::wstring& output, int& error)
{
	output.clear();
	if (input.empty())
	{
		error = IDNA_INVALID_NAME_SIZE;
		return false;
	}

	wchar_t label[MAX_LABEL_LEN];
	string::size_type inputLen = input.length();
	bool finalDot = false;
	if (input[inputLen] == '.')
	{
		inputLen--;
		finalDot = true;
	}

	string::size_type i = 0;
	while (i < inputLen)
	{
		bool appendDot = true;
		string::size_type pos = input.find('.', i);
		if (pos == string::npos)
		{
			pos = inputLen;
			appendDot = finalDot;
		}
		size_t labelLen = pos - i;
		if (!labelLen || labelLen > MAX_LABEL_LEN)
		{
			error = IDNA_INVALID_LABEL_SIZE;
			return false;
		}

		if (labelLen >= 4 && input[i+2] == '-' && input[i+3] == '-' &&
		    (input[i] == 'x' || input[i] == 'X') &&
		    (input[i+1] == 'n' || input[i+1] == 'N'))
		{
			labelLen -= 4;
			if (!labelLen)
			{
				error = IDNA_INVALID_LABEL_SIZE;
				return false;
			}
			size_t outputLen = MAX_LABEL_LEN;
			punycode_status status = punycode_decode(labelLen, input.c_str() + i + 4, outputLen, label);
			if (status != punycode_success)
			{
				error = IDNA_CONVERSION_ERROR;
				return false;
			}
			output.append(label, outputLen);
		}
		else
		{
			for (size_t j = i; j < pos; ++j)
			{
				if (input[j] <= ' ' || input[j] >= 128)
				{
					error = IDNA_BAD_CHARACTER;
					return false;
				}
				output += (wchar_t) input[j];
			}

		}
		if (appendDot)
			output += '.';
		if (output.length() > MAX_NAME_LEN)
		{
			error = IDNA_INVALID_NAME_SIZE;
			return false;
		}
		i = pos + 1;
	}
	error = 0;
	return true;
}

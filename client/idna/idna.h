#ifndef _IDNA_H
#define _IDNA_H

#include <string>

enum
{
	IDNA_INVALID_LABEL_SIZE = 1,
	IDNA_INVALID_NAME_SIZE,
	IDNA_BAD_CHARACTER,
	IDNA_CONVERSION_ERROR
};

bool IDNA_convert_to_ACE(const std::wstring& input, std::string& output, int& error);
bool IDNA_convert_from_ACE(const std::string& input, std::wstring& output, int& error);

#endif  /* _IDNA_H */


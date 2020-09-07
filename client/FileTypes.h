#ifndef FILE_TYPES_H
#define FILE_TYPES_H

#include <string>

enum
{
	FILE_TYPE_ANY = 0,
	FILE_TYPE_AUDIO,
	FILE_TYPE_COMPRESSED,
	FILE_TYPE_DOCUMENT,
	FILE_TYPE_EXECUTABLE,
	FILE_TYPE_IMAGE,
	FILE_TYPE_VIDEO,
	FILE_TYPE_DIRECTORY,
	FILE_TYPE_TTH,
	// flylinkdc++
	FILE_TYPE_CD_DVD,
	FILE_TYPE_COMICS,
	FILE_TYPE_EBOOK,
	NUMBER_OF_FILE_TYPES
};

unsigned int getFileTypesFromExt(const char* str, unsigned int len);
unsigned int getFileTypesFromFileName(const std::string& name);

#endif /* FILE_TYPES_H */

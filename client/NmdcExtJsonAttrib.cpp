#include "stdinc.h"
#include "NmdcExtJson.h"
#include "debug.h"

using namespace NmdcExtJson;

/* Command-line: gperf -C -I -L ANSI-C -G keywords-sorted.txt  */
/* Computed positions: -k'1,4' */

#define TOTAL_KEYWORDS 14
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 6
#define MAX_HASH_VALUE 27
/* maximum key range = 22, duplicates = 0 */

static inline unsigned int hash(const char *str, unsigned int len) noexcept
{
	static const unsigned char asso_values[] =
	{
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		5,  0,  0, 28, 28, 28, 10, 28, 28, 28,
		0,  5, 15,  0, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		0,  0, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 10, 28,  0, 28,  5,  0, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 28, 28
	};
	unsigned int hval = len;
	switch (hval)
	{
		default:
			hval += asso_values[(unsigned char)str[3]];
			/*FALLTHROUGH*/
		case 3:
		case 2:
		case 1:
			hval += asso_values[(unsigned char)str[0]];
			break;
	}
	return hval;
}

static const char* wordList[] =
{
	"", "", "", "", "", "",
	"Gender",
	"SQLSize",
	"StartGUI",
	"StartCore",
	"Files",
	"",
	"SQLFree",
	"QueueSrc",
	"",
	"QueueFiles",
	"",
	"Support",
	"RAM",
	"", "",
	"LDBHistSize",
	"RAMPeak",
	"LastDate",
	"", "", "",
	"RAMFree"
};

static const int8_t wordListValues[] =
{
	-1, -1, -1, -1, -1, -1,
	EXT_JSON_GENDER,
	EXT_JSON_SQL_SIZE,
	EXT_JSON_START_GUI,
	EXT_JSON_START_CORE,
	EXT_JSON_FILES,
	-1,
	EXT_JSON_SQL_FREE,
	EXT_JSON_QUEUE_SRC,
	-1,
	EXT_JSON_QUEUE_FILES,
	-1,
	EXT_JSON_SUPPORT,
	EXT_JSON_RAM,
	-1, -1,
	EXT_JSON_LDB_HIST_SIZE,
	EXT_JSON_RAM_PEAK,
	EXT_JSON_LAST_DATE,
	-1, -1, -1,
	EXT_JSON_RAM_FREE
};

static const string wordListSorted[] =
{
	"Files",
	"Gender",
	"LastDate",
	"LDBHistSize",
	"QueueFiles",
	"QueueSrc",
	"RAM",
	"RAMFree",
	"RAMPeak",
	"SQLFree",
	"SQLSize",
	"StartCore",
	"StartGUI",
	"Support"
};

const string& NmdcExtJson::getAttribText(int index) noexcept
{
	static_assert(NUM_EXT_JSON == _countof(wordListSorted), "Invalid word list");
	dcassert(index >= 0 && index < NUM_EXT_JSON);
	return wordListSorted[index];
}

int NmdcExtJson::getAttribByName(const string& str) noexcept
{
	static_assert(_countof(wordListValues) == _countof(wordList), "Invalid word list");
	string::size_type len = str.length();
	if (len > MAX_WORD_LENGTH || len < MIN_WORD_LENGTH) return -1;
	unsigned key = hash(str.c_str(), len);
	if (key > MAX_HASH_VALUE) return -1;
	return strcmp(wordList[key], str.c_str()) ? -1 : wordListValues[key];
}

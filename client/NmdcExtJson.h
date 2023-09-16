#ifndef NMDC_EXT_JSON_H_
#define NMDC_EXT_JSON_H_

#include "typedefs.h"

class JsonFormatter;

namespace NmdcExtJson
{

	enum
	{
		EXT_JSON_FILES,
		EXT_JSON_GENDER,
		EXT_JSON_LAST_DATE,
		EXT_JSON_LDB_HIST_SIZE,
		EXT_JSON_QUEUE_FILES,
		EXT_JSON_QUEUE_SRC,
		EXT_JSON_RAM,
		EXT_JSON_RAM_FREE,
		EXT_JSON_RAM_PEAK,
		EXT_JSON_SQL_FREE,
		EXT_JSON_SQL_SIZE,
		EXT_JSON_START_CORE,
		EXT_JSON_START_GUI,
		EXT_JSON_SUPPORT,
		NUM_EXT_JSON
	};


	int getAttribByName(const string& str) noexcept;
	const string& getAttribText(int index) noexcept;

	void appendStringAttrib(JsonFormatter& jf, int index, const string& value) noexcept;
	void appendIntAttrib(JsonFormatter& jf, int index, int value) noexcept;
	void appendInt64Attrib(JsonFormatter& jf, int index, int64_t value) noexcept;

	struct Data
	{
		string attr[NUM_EXT_JSON];
		bool parse(const string& json) noexcept;
	};

}

#endif // NMDC_EXT_JSON_H_

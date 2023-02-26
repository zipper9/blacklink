#ifndef WEB_SERVER_UTIL_H_
#define WEB_SERVER_UTIL_H_

#include "typedefs.h"
#include "ResourceManager.h"

namespace WebServerUtil
{
	struct TableInfo
	{
		int columnCount;
		const int* columnNames;
		const uint8_t* columnWidths;
		const char* url;
		const char* tableName;
	};

	struct TablePageInfo
	{
		int pages;
		size_t start;
		size_t count;
	};

	struct ActionInfo
	{
		enum
		{
			FLAG_TOOLTIP       = 1,
			FLAG_DOWNLOAD_LINK = 2
		};

		const char* icon;
		const char* onClick;
		const char* suffix;
		int flags;
	};

	enum
	{
		WIDTH_WRAP_CONTENT,
		WIDTH_SMALL,
		WIDTH_MEDIUM,
		WIDTH_LARGE,
		WIDTH_EXTRA_LARGE,
	};

	void getTableRange(int page, size_t totalItems, size_t pageSize, TablePageInfo& inf) noexcept;
	void printTableHeader(string& os, const TableInfo& ti, int sortColumn) noexcept;
	void printTableFooter(string& os, const TableInfo& ti) noexcept;
	void printTableCell(string& os, int& column, const TableInfo& ti, const string& data, bool escape) noexcept;
	void printPageSelector(string& os, int total, int current, const TableInfo& ti) noexcept;
	void printItemCount(string& os, int count, ResourceManager::Strings resEmpty, ResourceManager::Strings resPlural) noexcept;
	void printActions(string& os, int count, const ActionInfo* act, const string* data, const string& rowId) noexcept;
	void printSelector(string& os, const char* name, int count, const ResourceManager::Strings* resId, int startVal, int selVal) noexcept;
	string printItemId(uintptr_t id) noexcept;
	uintptr_t parseItemId(const string& s) noexcept;
	string getContentTypeForExt(const string& ext) noexcept;
	string getContentTypeForFile(const string& file) noexcept;
	string getStringQueryParam(const std::map<string, string>* query, const string& param) noexcept;
	int getIntQueryParam(const std::map<string, string>* query, const string& param, int defValue = 0) noexcept;
	string getStringByName(const string& name) noexcept;
	void expandLangStrings(string& data) noexcept;
	void expandCssVariables(string& data, const StringMap& vars) noexcept;
	void loadCssVariables(const string& data, StringMap& vars) noexcept;
}

#endif // WEB_SERVER_UTIL_H_

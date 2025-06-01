#include "stdinc.h"
#include "WebServerUtil.h"
#include "UriUtil.h"
#include "SimpleXML.h"
#include "Base32.h"

static const boost::unordered_map<string, string> extToContentType =
{
	{ "html", "text/html"                },
	{ "htm",  "text/html"                },
	{ "css",  "text/css"                 },
	{ "txt",  "text/plain"               },
	{ "js",   "application/javascript"   },
	{ "png",  "image/png"                },
	{ "gif",  "image/gif"                },
	{ "jpg",  "image/jpeg"               },
	{ "jpeg", "image/jpeg"               },
	{ "svg",  "image/svg+xml"            },
	{ "ico",  "image/vnd.microsoft.icon" }
};

static const string octetStream = "application/octet-stream";

void WebServerUtil::getTableRange(int page, size_t totalItems, size_t pageSize, TablePageInfo& inf) noexcept
{
	inf.pages = static_cast<int>((totalItems + pageSize - 1) / pageSize);
	page = std::max(1, page);
	page = std::min(inf.pages, page);
	if (!page)
	{
		inf.start = 0;
		inf.count = 0;
	}
	else
	{
		inf.start = pageSize * (page - 1);
		inf.count = std::min(pageSize, totalItems - inf.start);
	}
}

void WebServerUtil::printTableHeader(string& os, const TableInfo& ti, int sortColumn) noexcept
{
	os += "<tr class='header'>";
	for (int i = 0; i < ti.columnCount; ++i)
	{
		os += "<th";
		string cls;
		if (i == 0) cls = "r1";
		else if (i == ti.columnCount-1) cls = "r2";
		if (ti.columnNames[i] != -1)
		{
			if (!cls.empty()) cls += ' ';
			cls += "sortable";
		}
		if (!cls.empty())
		{
			os += " class='";
			os += cls;
			os += '\'';
		}
		if (ti.columnNames[i] != -1)
		{
			bool currentSort = abs(sortColumn) == i + 1;
			string sortLinkId = ti.tableName;
			sortLinkId += "-col";
			sortLinkId += Util::toString(i);
			sortLinkId += "-sort";
			os += " onclick='sortTable(\"";
			os += sortLinkId;
			os += "\")'";
			os += '>';
			os += "<a id='";
			os += sortLinkId;
			os += "' href='/";
			os += ti.url;
			os += "?sort=";
			os += Util::toString(currentSort ? -sortColumn : i + 1);
			os += "'>";
			os += ResourceManager::getString((ResourceManager::Strings) ti.columnNames[i]);
			if (currentSort)
				os += sortColumn > 0 ? " \xE2\x86\x93" : " \xE2\x86\x91";
			os += "</a>";
		}
		else
			os += '>';
		os += "</th>";
	}
	os += "</tr>\n";
}

void WebServerUtil::printTableFooter(string& os, const TableInfo& ti) noexcept
{
	os += "<tr>";
	for (int i = 0; i < ti.columnCount; ++i)
	{
		os += "<th";
		if (i == 0) os += " class='r3'";
		else if (i == ti.columnCount-1) os += " class='r4'";
		os += "></th>";
	}
	os += "</tr>\n";
}

void WebServerUtil::printTableCell(string& os, int& column, const TableInfo& ti, const string& data, bool escape) noexcept
{
	os += "<td class='";
	int width = ti.columnWidths[column];
	if (column == ti.columnCount-1)
	{
		os += "r-border ";
		column = -1;
	}
	++column;
	os += 'w';
	os += Util::toString(width);
	os += "\'>";
	if (escape)
	{
		string tmp;
		os += SimpleXML::escape(data, tmp, false);
	}
	else
		os += data;
	os += "</td>";
}

void WebServerUtil::printPageSelector(string& os, int total, int current, const TableInfo& ti) noexcept
{
	static const int NUM_FIRST = 3;
	static const int NUM_LAST = 3;
	static const int NUM_MIDDLE = 2;
	int ranges[6];
	if (current < 1)
		current = 1;
	else if (current > total)
		current = total;
	int currentStart = std::max(current - NUM_MIDDLE, 1);
	int currentEnd = std::min(current + NUM_MIDDLE, total);
	int lastStart = std::max(total - NUM_LAST + 1, 1);
	int firstEnd = std::min(NUM_FIRST, total);
	ranges[0] = 1;
	int index;
	if (currentStart > firstEnd + 1)
	{
		ranges[1] = firstEnd;
		ranges[2] = currentStart;
		ranges[3] = currentEnd;
		index = 3;
	}
	else
	{
		ranges[1] = std::max(currentEnd, firstEnd);
		index = 1;
	}
	if (lastStart > ranges[index] + 1)
	{
		ranges[++index] = lastStart;
		ranges[++index] = total;
	}
	else
		ranges[index] = total;
	bool first = true;
	os += "<div class='pages'>";
	for (int i = 0; i < index; i += 2)
	{
		if (i) os += " ...";
		for (int j = ranges[i]; j <= ranges[i+1]; ++j)
		{
			string pageStr = Util::toString(j);
			if (!first) os += ' ';
			if (j == current)
				os += "<span class='page-current'>" + pageStr + "</span>";
			else
				os += "<a href='/" + string(ti.url) + "?p=" + pageStr + "'>" + pageStr + "</a>";
			first = false;
		}
	}
	os += "</div>";
}

void WebServerUtil::printItemCount(string& os, int count, ResourceManager::Strings resEmpty, ResourceManager::Strings resPlural) noexcept
{
	os += "<h3>";
	string tmp;
	string text = count == 0 ?
		STRING_I(resEmpty) :
		(F_(ResourceManager::getPluralString(resPlural, ResourceManager::getPluralCategory(count))) % count).str();
	os += SimpleXML::escape(text, tmp, false);
	os += "</h3>";
}

void WebServerUtil::printActions(string& os, int count, const ActionInfo* act, const string* data, const string& rowId) noexcept
{
	string tmp;
	os += "<div class='actions'>\n";
	for (int i = 0; i < count; i++)
	{
		os += "<a href='";
		if (!(act[i].flags & ActionInfo::FLAG_TOOLTIP) && !data[i].empty())
			os += SimpleXML::escape(data[i], tmp, true);
		else
			os += '#';
		os += '\'';
		if (act[i].suffix)
		{
			os += " id='";
			os += rowId;
			os += '-';
			os += act[i].suffix;
			os += '\'';
		}
		if (act[i].onClick)
		{
			os += " onclick='";
			os += act[i].onClick;
			os += "(\"";
			os += rowId;
			os += "\")'";
		}
		if (act[i].flags & ActionInfo::FLAG_DOWNLOAD_LINK)
			os += " target='_blank'";
		os += "><img src='";
		os += act[i].icon;
		os += "'>";
		if ((act[i].flags & ActionInfo::FLAG_TOOLTIP) && !data[i].empty())
		{
			os += "<div class='tooltip'><span style='white-space: pre;'>";
			os += SimpleXML::escape(data[i], tmp, false);
			os += "</span></div>";
		}
		os += "</a>\n";
	}
	os += "</div>\n";
}

void WebServerUtil::printSelector(string& os, const char* name, int count, const ResourceManager::Strings* resId, int startVal, int selVal) noexcept
{
	string tmp;
	os += "<select class='field' name='";
	os += name;
	os += "'>";
	for (int i = 0; i < count; ++i)
	{
		os += "<option value='" + Util::toString(startVal + i) + "'";
		if (startVal + i == selVal) os += " selected='selected'";
		os += ">";
		os += SimpleXML::escape(STRING_I(resId[i]), tmp, false);
		os += "</option>";
	}
	os += "</select>";
}

string WebServerUtil::printItemId(uintptr_t id) noexcept
{
	uint8_t data[sizeof(id)];
	size_t count = 0;
	while (id)
	{
		data[count++] = id & 0xFF;
		id >>= 8;
	}
	if (!count) data[count++] = 0;
	return Util::toBase32(data, count);
}

uintptr_t WebServerUtil::parseItemId(const string& s) noexcept
{
	if (s.empty()) return 0;
	uint8_t data[sizeof(uintptr_t)];
	bool error;
	Util::fromBase32(s.c_str(), data, sizeof(data), &error);
	if (error) return 0;
	uintptr_t result = 0;
	for (int i = sizeof(data)-1; i >= 0; i--)
		result = result << 8 | data[i];
	return result;
}

string WebServerUtil::getContentTypeForExt(const string& ext) noexcept
{
	string s = ext;
	Text::asciiMakeLower(s);
	auto i = extToContentType.find(s);
	return i != extToContentType.end() ? i->second : octetStream;
}

string WebServerUtil::getContentTypeForFile(const string& file) noexcept
{
	auto pos = file.rfind('.');
	if (pos == string::npos || file.length() - pos > 8) return octetStream;
	string s = file.substr(pos + 1);
	Text::asciiMakeLower(s);
	auto i = extToContentType.find(s);
	return i != extToContentType.end() ? i->second : octetStream;
}

string WebServerUtil::getStringQueryParam(const std::map<string, string>* query, const string& param) noexcept
{
	if (!query) return Util::emptyString;
	auto i = query->find(param);
	if (i == query->end()) return Util::emptyString;
	return Util::decodeUri(i->second);
}

int WebServerUtil::getIntQueryParam(const std::map<string, string>* query, const string& param, int defValue) noexcept
{
	if (!query) return defValue;
	auto i = query->find(param);
	return i == query->end() ? defValue : Util::toInt(i->second);
}

void WebServerUtil::expandLangStrings(string& data) noexcept
{
	string::size_type i = 0;
	string value;
	while (i < data.length())
	{
		string::size_type j = data.find('%', i);
		if (j == string::npos || j == data.length()-1) break;
		if (data[j+1] == '[')
		{
			string::size_type k = data.find(']', j+2);
			if (k == string::npos) break;
			size_t len = k-(j+2);
			string var = data.substr(j+2, len);
			if (!var.empty()) value = getStringByName(var); else value.clear();
			data.erase(j, len+3);
			data.insert(j, value);
			i = j + value.length();
		}
		else
			i = j + 1;
	}
}

string WebServerUtil::getStringByName(const string& data) noexcept
{
	int str = ResourceManager::getStringByName(data);
	return str < 0 ? Util::emptyString : ResourceManager::getString((ResourceManager::Strings) str);
}

void WebServerUtil::expandCssVariables(string& data, const StringMap& vars) noexcept
{
	string::size_type i = 0;
	while (i < data.length())
	{
		string::size_type j = data.find('$', i);
		if (j == string::npos) break;
		const char* s = data.c_str();
		string::size_type len = data.length();
		string::size_type k = j + 1;
		while (k < len)
		{
			if (!((s[k] >= 'a' && s[k] <= 'z') || (s[k] >= 'A' && s[k] <= 'Z') ||
				(s[k] >= '0' && s[k] <= '9') || s[k] == '-' || s[k] == '_')) break;
			++k;
		}
		len = k - (j + 1);
		if (len)
		{
			string var = data.substr(j+1, len);
			data.erase(j, len+1);
			auto it = vars.find(var);
			if (it != vars.end())
			{
				const string& value = it->second;
				data.insert(j, value);
				i = j + value.length();
			}
		}
		else
			i = j + 1;
	}
}

void WebServerUtil::loadCssVariables(const string& data, StringMap& vars) noexcept
{
	vars.clear();
	const char* s = data.c_str();
	string::size_type len = data.length();
	string::size_type i = 0;
	while (i < len)
	{
		string::size_type j = data.find('$', i);
		if (j == string::npos) break;
		string::size_type k = j + 1;
		while (k < len)
		{
			if (!((s[k] >= 'a' && s[k] <= 'z') || (s[k] >= 'A' && s[k] <= 'Z') ||
				(s[k] >= '0' && s[k] <= '9') || s[k] == '-' || s[k] == '_')) break;
			++k;
		}
		string::size_type vlen = k - (j + 1);
		if (vlen)
		{
			string v = data.substr(j+1, vlen);
			while (k < len && (s[k] == ' ' || s[k] == '\t' || s[k] == '\r' || s[k] == '\n')) ++k;
			if (k == len) break;
			if (s[k] != ':')
			{
				i = k + 1;
				continue;
			}
			++k;
			j = data.find(';', k);
			if (j == string::npos) break;
			vars[v] = data.substr(k, j-k);
		}
		i = j + 1;
	}
}

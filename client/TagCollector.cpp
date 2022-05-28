#include "stdinc.h"
#include "TagCollector.h"

bool TagCollector::addTag(const string& tag, const string& source)
{
	LOCK(csData);
	if (data.size() >= MAX_TAGS) return false;
	TagData& td = data[tag];
	if (td.count == MAX_SOURCES) return false;
	for (size_t i = 0; i < td.count; ++i)
		if (td.sources[i] == source) return false;
	td.sources[td.count++] = source;
	return true;
}

void TagCollector::clear()
{
	LOCK(csData);
	data.clear();
}

string TagCollector::getInfo() const
{
	string s;
	LOCK(csData);
	for (auto i = data.cbegin(); i != data.cend(); ++i)
	{
		const TagData& td = i->second;
		s += '\t';
		s += i->first;
		s += ": ";
		for (size_t j = 0; j < td.count; ++j)
		{
			if (j) s += ", ";
			auto pos = td.sources[j].find('\t');
			if (pos != string::npos)
			{
				s += td.sources[j].substr(0, pos);
				s += " on ";
				s += td.sources[j].substr(pos + 1);
			}
			else
				s += td.sources[j];
		}
		s += '\n';
	}
	return s;
}

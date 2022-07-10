#include "stdafx.h"
#include "SearchHistory.h"
#include "../client/DatabaseManager.h"
#include "../client/StrUtil.h"

void SearchHistory::addItem(const tstring& s, int maxCount)
{
	if (s.empty()) return;
	data.remove(s);
	if (--maxCount < 0) maxCount = 0;
	while (data.size() > TStringList::size_type(maxCount))
		data.pop_back();
	data.push_front(s);
}

void SearchHistory::load(DBRegistryType type)
{
	DBRegistryMap values;
	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	if (conn) conn->loadRegistry(values, type);
	data.clear();
	unsigned key = 0;
	while (true)
	{
		auto i = values.find(Util::toString(++key));
		if (i == values.end()) break;
		data.push_back(Text::toT(i->second.sval));
	}
}

void SearchHistory::firstLoad(DBRegistryType type)
{
	if (!firstLoadFlag) return;
	load(type);
	firstLoadFlag = false;
}

void SearchHistory::save(DBRegistryType type)
{
	DBRegistryMap values;
	unsigned key = 0;
	for (auto i = data.cbegin(); i != data.cend(); ++i)
		values.insert(DBRegistryMap::value_type(Util::toString(++key), DBRegistryValue(Text::fromT(*i))));
	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	if (conn)
	{
		conn->clearRegistry(type, 0);
		conn->saveRegistry(values, type, true);
	}
}

void SearchHistory::clear(DBRegistryType type)
{
	data.clear();
	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	if (conn) conn->clearRegistry(type, 0);
}

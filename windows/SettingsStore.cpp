#include "stdafx.h"
#include "SettingsStore.h"

void SettingsStore::setIntValue(int id, int value)
{
	intData[id] = value;
}

void SettingsStore::setStrValue(int id, const string& value)
{
	strData[id] = value;
}

bool SettingsStore::getIntValue(int id, int& value) const
{
	auto i = intData.find(id);
	if (i == intData.end()) return false;
	value = i->second;
	return true;
}

bool SettingsStore::getBoolValue(int id, bool& value) const
{
	int ival;
	if (getIntValue(id, ival))
	{
		value = ival != 0;
		return true;
	}
	return false;
}

bool SettingsStore::getStrValue(int id, string& value) const
{
	auto i = strData.find(id);
	if (i == strData.end()) return false;
	value = i->second;
	return true;
}

void SettingsStore::unsetIntvalue(int id)
{
	auto i = intData.find(id);
	if (i != intData.end()) intData.erase(i);
}

void SettingsStore::unsetStrValue(int id)
{
	auto i = strData.find(id);
	if (i != strData.end()) strData.erase(i);
}

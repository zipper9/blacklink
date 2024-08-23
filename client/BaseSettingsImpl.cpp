#include "stdinc.h"
#include "BaseSettingsImpl.h"
#include "BaseUtil.h"
#include "debug.h"

#ifdef DEBUG_SETTINGS
#include "Util.h"
#define ASSERT_ARG 0
#define CHECK_THREAD(id) do { if (id >= 1024) ASSERT_MAIN_THREAD(); } while(0)
#else
#define ASSERT_ARG 1
#define CHECK_THREAD(id)
#endif

using std::string;

void BaseSettingsImpl::addInt(int id, const string& name, int def, int flags, Validator<int>* validator)
{
	ValueHolder<int> vh;
	vh.name = name;
	vh.val = vh.def = def;
	vh.flags = flags & ~FLAG_VALUE_CHANGED;
	vh.validator = validator;
	is.insert(std::make_pair(id, vh));
}

void BaseSettingsImpl::addBool(int id, const string& name, bool def)
{
	ValueHolder<int> vh;
	vh.name = name;
	vh.val = vh.def = def ? 1 : 0;
	vh.flags = FLAG_BOOL;
	vh.validator = nullptr;
	is.insert(std::make_pair(id, vh));
}

void BaseSettingsImpl::addString(int id, const string& name, const string& def, int flags, Validator<string>* validator)
{
	ValueHolder<string> vh;
	vh.name = name;
	vh.def = def;
	vh.flags = flags & ~FLAG_VALUE_CHANGED;
	vh.validator = validator;
	ss.insert(std::make_pair(id, vh));
}

bool BaseSettingsImpl::getInt(int id, int& val) const
{
	CHECK_THREAD(id);
	auto i = is.find(id);
	if (i == is.end())
	{
		dcassert(ASSERT_ARG);
		return false;
	}
	const auto& data = i->second;
	if (data.flags & FLAG_VALUE_CHANGED)
		val = data.val;
	else
		val = data.def;
	return true;
}

int BaseSettingsImpl::getInt(int id, int defVal) const
{
	CHECK_THREAD(id);
	auto i = is.find(id);
	if (i == is.end())
	{
		dcassert(ASSERT_ARG);
		return defVal;
	}
	const auto& data = i->second;
	if (data.flags & FLAG_VALUE_CHANGED)
		return data.val;
	return data.def;
}

Settings::Result BaseSettingsImpl::setInt(int id, int val, int flags)
{
	CHECK_THREAD(id);
	auto i = is.find(id);
	if (i == is.end())
	{
		dcassert(ASSERT_ARG);
		return RESULT_NOT_FOUND;
	}
	auto& data = i->second;
	Result result = RESULT_OK;
	if (data.validator && !data.validator->checkValue(val, data.flags))
	{
		if (!(data.flags & FLAG_FIX_VALUE) && !(flags & SET_FLAG_FIX_VALUE)) return RESULT_UNCHANGED;
		data.validator->fixValue(val);
		result = RESULT_UPDATED;
	}
	data.val = val;
	data.flags |= FLAG_VALUE_CHANGED;
	return result;
}

void BaseSettingsImpl::unsetInt(int id)
{
	CHECK_THREAD(id);
	auto i = is.find(id);
	if (i != is.end()) i->second.flags &= ~FLAG_VALUE_CHANGED;
}

int BaseSettingsImpl::getIntDefault(int id) const
{
	auto i = is.find(id);
	return i != is.end() ? i->second.def : 0;
}

bool BaseSettingsImpl::getIntRange(int id, int& minVal, int& maxVal) const
{
	minVal = INT_MIN;
	maxVal = INT_MAX;
	auto i = is.find(id);
	if (i == is.end()) return false;
	auto validator = i->second.validator;
	if (!validator) return false;
	validator->fixValue(minVal);
	validator->fixValue(maxVal);
	return minVal <= maxVal;
}

bool BaseSettingsImpl::getBool(int id, bool defVal) const
{
	CHECK_THREAD(id);
	auto i = is.find(id);
	if (i == is.end())
	{
		dcassert(ASSERT_ARG);
		return defVal;
	}
	const auto& data = i->second;
	if (data.flags & FLAG_VALUE_CHANGED)
		return data.val != 0;
	return data.def != 0;
}

Settings::Result BaseSettingsImpl::setBool(int id, bool val)
{
	CHECK_THREAD(id);
	auto i = is.find(id);
	if (i == is.end())
	{
		dcassert(ASSERT_ARG);
		return RESULT_NOT_FOUND;
	}
	auto& data = i->second;
	data.val = val ? 1 : 0;
	data.flags |= FLAG_VALUE_CHANGED;
	return RESULT_OK;
}

bool BaseSettingsImpl::getString(int id, string& val) const
{
	CHECK_THREAD(id);
	auto i = ss.find(id);
	if (i == ss.end())
	{
		dcassert(ASSERT_ARG);
		return false;
	}
	auto& data = i->second;
	if (data.flags & FLAG_VALUE_CHANGED)
		val = data.val;
	else
		val = data.def;
	return true;
}

const string& BaseSettingsImpl::getString(int id) const
{
	CHECK_THREAD(id);
	auto i = ss.find(id);
	if (i == ss.end())
	{
		dcassert(ASSERT_ARG);
		return Util::emptyString;
	}
	auto& data = i->second;
	if (data.flags & FLAG_VALUE_CHANGED)
		return data.val;
	return data.def;
}

Settings::Result BaseSettingsImpl::setString(int id, const string& val, int flags)
{
	CHECK_THREAD(id);
	auto i = ss.find(id);
	if (i == ss.end())
	{
		dcassert(ASSERT_ARG);
		return RESULT_NOT_FOUND;
	}
	auto& data = i->second;
	Result result = RESULT_OK;
	if (data.validator && !data.validator->checkValue(val, data.flags))
	{
		if (!(data.flags & FLAG_FIX_VALUE) && !(flags & SET_FLAG_FIX_VALUE)) return RESULT_UNCHANGED;
		string newVal = val;
		data.validator->fixValue(newVal);
		data.val = std::move(newVal);
		result = RESULT_UPDATED;
	}
	else
		data.val = val;
	if (!(data.flags & FLAG_ALLOW_EMPTY_STRING) && data.val.empty())
		data.flags &= ~FLAG_VALUE_CHANGED;
	else
		data.flags |= FLAG_VALUE_CHANGED;
	return result;
}

void BaseSettingsImpl::unsetString(int id)
{
	CHECK_THREAD(id);
	auto i = ss.find(id);
	if (i != ss.end()) i->second.flags &= ~FLAG_VALUE_CHANGED;
}

const string& BaseSettingsImpl::getStringDefault(int id) const
{
	auto i = ss.find(id);
	return i != ss.end() ? i->second.def : Util::emptyString;
}

void BaseSettingsImpl::setStringDefault(int id, const std::string& s)
{
	CHECK_THREAD(id);
	auto i = ss.find(id);
	if (i != ss.end()) i->second.def = s;
}

void BaseSettingsImpl::initMaps(NameToInfoMap& nameMap, IdToInfoMap& idMap)
{
	for (auto& i : is)
	{
		const string& name = i.second.name;
		int flags = i.second.flags;
		auto si = std::make_shared<SettingInfo>(SettingInfo{this, i.first,
			(flags & FLAG_BOOL) ? TYPE_BOOL : TYPE_INT, flags, name});
		nameMap.insert(std::make_pair(name, si));
		idMap.insert(std::make_pair(i.first, si));
	}
	for (auto& i : ss)
	{
		const string& name = i.second.name;
		auto si = std::make_shared<SettingInfo>(SettingInfo{this, i.first, TYPE_STRING, i.second.flags, name});
		nameMap.insert(std::make_pair(name, si));
		idMap.insert(std::make_pair(i.first, si));
	}
}

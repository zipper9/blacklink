#include "stdinc.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include "SimpleXML.h"
#include "StrUtil.h"
#include "AppPaths.h"
#include "Path.h"
#include "File.h"
#include "version.h"

using std::string;

SettingsManager SettingsManager::instance;

SettingsManager::SettingsManager()
{
	coreSettings.reset(new ThreadSafeSettingsImpl);
	uiSettings.reset(new BaseSettingsImpl);
}

Settings::SettingInfoPtr SettingsManager::getSettingByName(const string& name) const
{
	auto i = nameMap.find(name);
	if (i == nameMap.end()) return nullptr;
	return i->second;
}

Settings::SettingInfoPtr SettingsManager::getSettingById(int id) const
{
	auto i = idMap.find(id);
	if (i == idMap.end()) return nullptr;
	return i->second;
}

void SettingsManager::initSettings()
{
	if (!nameMap.empty()) return;
	coreSettings->initMaps(nameMap, idMap);
	uiSettings->initMaps(nameMap, idMap);
}

static const string& pathToRelative(const string& path, const string& prefix, string& tmp)
{
	if (!prefix.empty() && File::isAbsolute(path) && prefix.length() < path.length() && !strnicmp(path, prefix, prefix.length()))
	{
		tmp = path.substr(prefix.length());
		return tmp;
	}
	return path;
}

static const string& pathFromRelative(const string& path, const string& prefix, string& tmp)
{
	if (path.empty() || File::isAbsolute(path) || path[0] == PATH_SEPARATOR || prefix.empty()) return path;
	tmp = prefix;
	if (tmp.back() != PATH_SEPARATOR) tmp += PATH_SEPARATOR;
	tmp += path;
	return tmp;
}

static const string strType("type");
static const string strString("string");
static const string strInt("int");

void SettingsManager::loadSettings(SimpleXML& xml)
{
	initSettings();
	if (!xml.findChild("Settings"))
		throw SettingsException("Invalid settings file format");
	xml.stepIn();

	string modulePath = Util::getExePath();
	if (!File::isAbsolute(modulePath) || modulePath.length() == 3) modulePath.clear();

	string tmp;
	while (xml.getNextChild())
	{
		const string& name = xml.getChildTag();
		auto i = nameMap.find(name);
		if (i == nameMap.end()) continue;
		const Settings::SettingInfo* si = i->second.get();
		const string& data = xml.getChildData();
		switch (si->type)
		{
			case Settings::TYPE_INT:
				si->settings->setInt(si->id, Util::toInt(data));
				break;
			case Settings::TYPE_BOOL:
				si->settings->setBool(si->id, data != "0");
				break;
			case Settings::TYPE_STRING:
				if (si->flags & Settings::FLAG_CONVERT_PATH)
					si->settings->setString(si->id, pathFromRelative(data, modulePath, tmp));
				else
					si->settings->setString(si->id, data);
		}
	}
	xml.stepOut();
	xml.resetCurrentChild();
}

void SettingsManager::saveSettings(SimpleXML& xml, BaseSettingsImpl* settings) const
{
	string modulePath = Util::getExePath();
	if (!File::isAbsolute(modulePath) || modulePath.length() == 3) modulePath.clear();

	string tmp;
	for (const auto& item : settings->ss)
	{
		const auto& vh = item.second;
		if (!(vh.flags & BaseSettingsImpl::FLAG_VALUE_CHANGED)) continue;
		if (vh.flags & Settings::FLAG_CONVERT_PATH)
			xml.addTag(vh.name, pathToRelative(vh.val, modulePath, tmp));
		else
			xml.addTag(vh.name, vh.val);
		xml.addChildAttrib(strType, strString);
	}

	for (const auto& item : settings->is)
	{
		const auto& vh = item.second;
		if (!(vh.flags & BaseSettingsImpl::FLAG_VALUE_CHANGED)) continue;
		xml.addTag(vh.name, Util::toString(vh.val));
		xml.addChildAttrib(strType, strInt);
	}
}

void SettingsManager::saveSettings(SimpleXML& xml) const
{
	ThreadSafeSettingsImpl* coreSettingsPtr = coreSettings.get();
	coreSettingsPtr->lockRead();
	try
	{
		saveSettings(xml, coreSettingsPtr);
	}
	catch (Exception&)
	{
		coreSettingsPtr->unlockRead();
		throw;
	}
	coreSettingsPtr->unlockRead();

	saveSettings(xml, uiSettings.get());
}

string SettingsManager::getConfigFile()
{
	return Util::getConfigPath() + "DCPlusPlus.xml";
}

void SettingsManager::loadSettings()
{
	Util::migrate(getConfigFile());
	try
	{
		SimpleXML xml;
		const string fileData = File(getConfigFile(), File::READ, File::OPEN).read();
		xml.fromXML(fileData);
		xml.stepIn();
		loadSettings(xml);
		fire(SettingsManagerListener::Load(), xml);
	}
	catch (const Exception&)
	{
	}
}

void SettingsManager::saveSettings()
{
	SimpleXML xml;
	xml.addTag("DCPlusPlus");
	xml.stepIn();
	xml.addTag("Settings");
	xml.stepIn();
	xml.addTag("ConfigVersion", VERSION_STR);
	xml.addChildAttrib(strType, strString);

	saveSettings(xml);
	xml.stepOut();

	fire(SettingsManagerListener::Save(), xml);
	fire(SettingsManagerListener::ApplySettings());

	string fileName = getConfigFile();
	try
	{
		string tempFile = fileName + ".tmp";
		File out(tempFile, File::WRITE, File::CREATE | File::TRUNCATE);
		BufferedOutputStream<false> f(&out, 64 * 1024);
		f.write(SimpleXML::utf8Header);
		xml.toXML(&f);
		f.flushBuffers(true);
		out.close();
		File::renameFile(tempFile, fileName);
	}
	catch (const FileException& e)
	{
		LogManager::message("Error saving settings to " + fileName + ", " + e.getError());
	}
}

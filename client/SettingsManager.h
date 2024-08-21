#ifndef SETTINGS_MANAGER_H_
#define SETTINGS_MANAGER_H_

#include "SettingsManagerListener.h"
#include "BaseSettingsImpl.h"
#include "ThreadSafeSettingsImpl.h"
#include "Exception.h"
#include "Speaker.h"
#include <memory>

class SimpleXML;

STANDARD_EXCEPTION(SettingsException);

class SettingsManager : public Speaker<SettingsManagerListener>
{
public:
	SettingsManager();

	SettingsManager(const SettingsManager&) = delete;
	SettingsManager& operator= (const SettingsManager&) = delete;

	BaseSettingsImpl* getCoreSettings() const { return coreSettings.get(); }
	BaseSettingsImpl* getUiSettings() const { return uiSettings.get(); }

	Settings::SettingInfoPtr getSettingByName(const std::string& name) const;
	Settings::SettingInfoPtr getSettingById(int id) const;
	void loadSettings(SimpleXML& xml);
	void saveSettings(SimpleXML& xml) const;
	void initSettings();
	void loadSettings();
	void saveSettings();
	static std::string getConfigFile();

	static SettingsManager instance;

private:
	Settings::NameToInfoMap nameMap;
	Settings::IdToInfoMap idMap;

	std::unique_ptr<ThreadSafeSettingsImpl> coreSettings;
	std::unique_ptr<BaseSettingsImpl> uiSettings;

	void saveSettings(SimpleXML& xml, BaseSettingsImpl* settings) const;
};

#endif // SETTINGS_MANAGER_H_

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <string>
#include <unordered_map>
#include <memory>

class Settings
{
public:
	enum
	{
		TYPE_INT,
		TYPE_BOOL,
		TYPE_STRING
	};

	enum
	{
		FLAG_BOOL               = 1,
		FLAG_CONVERT_PATH       = 2,
		FLAG_FIX_VALUE          = 4,
		FLAG_ALLOW_EMPTY_STRING = 8
	};

	enum
	{
		SET_FLAG_FIX_VALUE = 1
	};

	enum Result
	{
		RESULT_OK,
		RESULT_NOT_FOUND,
		RESULT_UNCHANGED,
		RESULT_UPDATED
	};

	virtual bool getInt(int id, int& val) const = 0;
	virtual int getInt(int id, int defVal = 0) const = 0;
	virtual Result setInt(int id, int val, int flags = 0) = 0;
	virtual void unsetInt(int id) = 0;
	virtual int getIntDefault(int id) const = 0;
	virtual bool getIntRange(int id, int& minVal, int& maxVal) const = 0;

	virtual bool getBool(int id, bool defVal = false) const = 0;
	virtual Result setBool(int id, bool val) = 0;

	virtual bool getString(int id, std::string& val) const = 0;
	virtual const std::string& getString(int id) const = 0;
	virtual Result setString(int id, const std::string& val, int flags = 0) = 0;
	virtual void unsetString(int id) = 0;
	virtual const std::string& getStringDefault(int id) const = 0;
	virtual void setStringDefault(int id, const std::string& s) = 0;

	virtual void lockRead() = 0;
	virtual void unlockRead() = 0;
	virtual void lockWrite() = 0;
	virtual void unlockWrite() = 0;

	struct SettingInfo
	{
		Settings* settings;
		int id;
		int type;
		int flags;
		std::string name;
	};

	using SettingInfoPtr = std::shared_ptr<SettingInfo>;
	using NameToInfoMap = std::unordered_map<std::string, SettingInfoPtr>;
	using IdToInfoMap = std::unordered_map<int, SettingInfoPtr>;
};

#endif // SETTINGS_H_

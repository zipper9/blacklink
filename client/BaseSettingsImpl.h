#ifndef BASE_SETTINGS_IMPL_H_
#define BASE_SETTINGS_IMPL_H_

#include "Settings.h"
#include <map>

class BaseSettingsImpl : public Settings
{
	friend class SettingsManager;

public:
	template<typename T>
	struct Validator
	{
		virtual bool checkValue(const T& val, int flags) const noexcept = 0;
		virtual void fixValue(T& val) const noexcept = 0;
	};

	template<typename T>
	struct MinMaxValidator : public Validator<T>
	{
		MinMaxValidator(T minVal, T maxVal) : minVal(minVal), maxVal(maxVal) {}

		virtual bool checkValue(const T& val, int flags) const noexcept
		{
			return val >= minVal && val <= maxVal;
		}
		virtual void fixValue(T& val) const noexcept
		{
			if (val < minVal) val = minVal;
			if (val > maxVal) val = maxVal;
		}

		private:
		const T minVal, maxVal;
	};

	template<typename T>
	struct MinMaxValidatorWithZero : public Validator<T>
	{
		MinMaxValidatorWithZero(T minVal, T maxVal) : minVal(minVal), maxVal(maxVal) {}

		virtual bool checkValue(const T& val, int flags) const noexcept
		{
			return !val || (val >= minVal && val <= maxVal);
		}
		virtual void fixValue(T& val) const noexcept
		{
			if (!val) return;
			if (val < minVal) val = minVal;
			if (val > maxVal) val = maxVal;
		}

		private:
		const T minVal, maxVal;
	};

	template<typename T>
	struct MinMaxValidatorWithDef : public Validator<T>
	{
		MinMaxValidatorWithDef(T minVal, T maxVal, T defVal) : minVal(minVal), maxVal(maxVal), defVal(defVal) {}

		virtual bool checkValue(const T& val, int flags) const noexcept
		{
			return val >= minVal && val <= maxVal;
		}
		virtual void fixValue(T& val) const noexcept
		{
			if (val < minVal || val > maxVal) val = defVal;
		}

		private:
		const T minVal, maxVal, defVal;
	};

	template<typename T>
	struct ListValidator : public Validator<T>
	{
		ListValidator(const T* values, int numValues) : values(values), numValues(numValues) {}

		virtual bool checkValue(const T& val, int flags) const noexcept
		{
			for (int i = 0; i < numValues; ++i)
				if (values[i] == val) return true;
			return false;
		}
		virtual void fixValue(T& val) const noexcept
		{
			if (!checkValue(val, 0)) val = values[0];
		}

		private:
		const T* const values;
		const int numValues;
	};

	struct StringSizeValidator : public Validator<string>
	{
		StringSizeValidator(size_t maxLen) : maxLen(maxLen) {}

		virtual bool checkValue(const std::string& val, int flags) const noexcept
		{
			return val.length() <= maxLen;
		}
		virtual void fixValue(std::string& val) const noexcept
		{
			if (val.length() > maxLen) val.erase(maxLen);
		}

		private:
		const size_t maxLen;
	};

private:
	enum
	{
		FLAG_VALUE_CHANGED = 0x100
	};

	template<typename T>
	struct ValueHolder
	{
		std::string name;
		T val;
		T def;
		int flags;
		Validator<T>* validator;
	};

	std::map<int, ValueHolder<int>> is;
	std::map<int, ValueHolder<std::string>> ss;

public:
	void addInt(int id, const std::string& name, int def = 0, int flags = 0, Validator<int>* validator = nullptr);
	void addBool(int id, const std::string& name, bool def = false);
	void addString(int id, const std::string& name, const std::string& def = std::string(), int flags = 0, Validator<std::string>* validator = nullptr);

	bool getInt(int id, int& val) const override;
	int getInt(int id, int defVal = 0) const override;
	Result setInt(int id, int val, int flags = 0) override;
	void unsetInt(int id) override;
	int getIntDefault(int id) const override;
	bool getIntRange(int id, int& minVal, int& maxVal) const override;

	bool getBool(int id, bool defVal = false) const override;
	Result setBool(int id, bool val) override;

	bool getString(int id, std::string& val) const override;
	const std::string& getString(int id) const override;
	Result setString(int id, const std::string& val, int flags = 0) override;
	void unsetString(int id) override;
	const std::string& getStringDefault(int id) const override;
	void setStringDefault(int id, const std::string& s) override;

	void lockRead() override {}
	void unlockRead() override {}
	void lockWrite() override {}
	void unlockWrite() override {}

	void initMaps(NameToInfoMap& nameMap, IdToInfoMap& idMap);
};

#endif // BASE_SETTINGS_IMPL_H_

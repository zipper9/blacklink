#ifndef SETTINGS_STORE_H_
#define SETTINGS_STORE_H_

#include <string>
#include <boost/unordered/unordered_map.hpp>

using std::string;

class SettingsStore
{
	public:
		void setIntValue(int id, int value);
		void setStrValue(int id, const string& value);
		bool getIntValue(int id, int& value) const;
		bool getBoolValue(int id, bool& value) const;
		bool getStrValue(int id, string& value) const;
		void unsetIntvalue(int id);
		void unsetStrValue(int id);

	private:
		boost::unordered_map<int, int> intData;
		boost::unordered_map<int, string> strData;
};

#endif // SETTINGS_STORE_H_

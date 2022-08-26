#ifndef PROP_PAGE_CALLBACK_H_
#define PROP_PAGE_CALLBACK_H_

struct PropPageCallback
{
	virtual void settingChanged(int id) = 0;
	virtual void intSettingChanged(int id, int value) = 0;
};

#endif // PROP_PAGE_CALLBACK_H_

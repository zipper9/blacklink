#ifndef THREAD_SAFE_SETTINGS_IMPL_H_
#define THREAD_SAFE_SETTINGS_IMPL_H_

#include "BaseSettingsImpl.h"
#include "RWLock.h"
#include <memory>

class ThreadSafeSettingsImpl : public BaseSettingsImpl
{
public:
	ThreadSafeSettingsImpl();

	void lockRead() override;
	void unlockRead() override;
	void lockWrite() override;
	void unlockWrite() override;

private:
	std::unique_ptr<RWLock> lock;
};

#endif // THREAD_SAFE_SETTINGS_IMPL_H_

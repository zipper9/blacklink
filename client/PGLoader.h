#ifndef PGLOADER_H
#define PGLOADER_H

#include "IpList.h"
#include "webrtc/rtc_base/synchronization/rw_lock_wrapper.h"

class PGLoader
{
	public:
		PGLoader();
		bool isBlocked(uint32_t addr) const noexcept;
		void load() noexcept;
		void clear() noexcept;
		static std::string getFileName();

	private:
		IpList ipList;
		mutable unique_ptr<webrtc::RWLockWrapper> cs;
		bool hasWhiteList;
};

extern PGLoader ipTrust;

#endif // PGLOADER_H


#ifndef UPLOAD_H_
#define UPLOAD_H_

#include "Transfer.h"
#include "Flags.h"

class InputStream;

class Upload : public Transfer, public Flags
{
	public:
		enum Flags
		{
			FLAG_ZUPLOAD = 0x01,
			FLAG_PENDING_KICK = 0x02,
			FLAG_RESUMED = 0x04,
			FLAG_CHUNKED = 0x08,
			FLAG_UPLOAD_PARTIAL = 0x10
		};
		
		Upload(UserConnection* conn, const TTHValue& tth, const string& path, const string& ip, const string& cipherName);
		~Upload();
		
		void getParams(StringMap& params) const;
		void updateSpeed(uint64_t currentTick);
		
		int64_t getAdjustedPos() const;
		int64_t getAdjustedActual() const;
		int64_t getSecondsLeft() const;

	private:
		GETSET(InputStream*, readStream, ReadStream);
		GETSET(uint64_t, tickForRemove, TickForRemove);
		GETSET(int64_t, downloadedBytes, DownloadedBytes);
};

typedef std::shared_ptr<Upload> UploadPtr;

#endif /*UPLOAD_H_*/


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
		
		Upload(const UserConnectionPtr& conn, const TTHValue& tth, const string& path, InputStream* is);
		~Upload();
		
		void getParams(StringMap& params) const;
		void updateSpeed(uint64_t currentTick);
		
		int64_t getAdjustedPos() const;
		int64_t getSecondsLeft() const;

		InputStream* getReadStream() const { return readStream; }

	private:
		InputStream* const readStream;

		GETSET(uint64_t, tickForRemove, TickForRemove);
		GETSET(int64_t, downloadedBytes, DownloadedBytes);

#ifdef DEBUG_SHUTDOWN
	public:
		static std::atomic<int> countCreated, countDeleted;
#endif
};

typedef std::shared_ptr<Upload> UploadPtr;

#endif /*UPLOAD_H_*/

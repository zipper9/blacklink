#ifndef DCPLUSPLUS_DCPP_DOWNLOAD_H_
#define DCPLUSPLUS_DCPP_DOWNLOAD_H_

#include "noexcept.h"
#include "Transfer.h"
#include "Streams.h"
#include "MerkleTree.h"
#include "Flags.h"

/**
 * Comes as an argument in the DownloadManagerListener functions.
 * Use it to retrieve information about the ongoing transfer.
 */
class AdcCommand;
class Download : public Transfer, public Flags
{
	public:
		enum
		{
			FLAG_ZDOWNLOAD        = 0x001,
			FLAG_CHUNKED          = 0x002,
			FLAG_TTH_CHECK        = 0x004,
			FLAG_SLOWUSER         = 0x008,
			FLAG_XML_BZ_LIST      = 0x010,
			FLAG_DOWNLOAD_PARTIAL = 0x020,
			FLAG_OVERLAP          = 0x040,
			FLAG_USER_CHECK       = 0x080,
			FLAG_RECURSIVE_LIST   = 0x100,
			FLAG_USER_GET_IP      = 0x200,
			FLAG_MATCH_QUEUE      = 0x400,
			FLAG_TTH_LIST         = 0x800
		};

		enum
		{
			REASON_CODE_UNSPECIFIED,
			REASON_CODE_CONNECTION_FAILURE,
			REASON_CODE_FILE_UNAVAILABLE,
			REASON_CODE_OTHER_ERROR // not used
		};

		Download(const UserConnectionPtr& conn, const QueueItemPtr& qi) noexcept;

		void getParams(StringMap& params) const;
		int64_t getSecondsLeft(bool wholeFile = false) const;

		~Download();

		/** @return Target filename without path. */
		string getTargetFileName() const;

		string getDownloadTarget() const;

		/** @internal */
		TigerTree& getTigerTree() { return tigerTree; }
		const TigerTree& getTigerTree() const { return tigerTree; }
		string& getFileListBuffer() { return fileListBuffer; }
		const QueueItemPtr& getQueueItem() const { return qi; }

		/** @internal */
		void getCommand(AdcCommand& cmd, bool zlib) const;

		string getTempTarget() const;

#ifdef BL_FEATURE_DROP_SLOW_SOURCES
		GETSET(uint64_t, lastNormalSpeed, LastNormalSpeed);
#endif
		void setDownloadFile(OutputStream* file)
		{
			downloadFile = file;
		}

		OutputStream* getDownloadFile()
		{
			return downloadFile;
		}

		GETSET(string, reasonText, ReasonText);
		GETSET(int, reasonCode, ReasonCode);
#ifdef DEBUG_TRANSFERS
		GETSET(string, downloadPath, DownloadPath);
#endif

		void resetDownloadFile()
		{
			delete downloadFile;
			downloadFile = nullptr;
		}

		int64_t getDownloadedBytes() const;
		void updateSpeed(uint64_t currentTick);

	private:
		OutputStream* downloadFile;
		const QueueItemPtr qi;
		TigerTree tigerTree;
		string fileListBuffer;

		static string getTargetPath(const QueueItemPtr& qi);

	public:
		struct ErrorInfo
		{
			int error;
			Type type;
			string target;
			int64_t size;
		};

#ifdef DEBUG_SHUTDOWN
		static std::atomic<int> countCreated, countDeleted;
#endif
};

typedef std::shared_ptr<Download> DownloadPtr;

#endif /*DOWNLOAD_H_*/

#ifndef DCPLUSPLUS_DCPP_DOWNLOAD_H_
#define DCPLUSPLUS_DCPP_DOWNLOAD_H_

#include "noexcept.h"
#include "Transfer.h"
#include "Streams.h"

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
			FLAG_ZDOWNLOAD          = 0x01,
			FLAG_CHUNKED            = 0x02,
			FLAG_TTH_CHECK          = 0x04,
			FLAG_SLOWUSER           = 0x08,
			FLAG_XML_BZ_LIST        = 0x10,
			FLAG_DOWNLOAD_PARTIAL   = 0x20,
			FLAG_OVERLAP            = 0x40,
#ifdef IRAINMAN_INCLUDE_USER_CHECK
			FLAG_USER_CHECK     = 0x80,
#endif
			FLAG_USER_GET_IP    = 0x200
		};
		
		Download(UserConnection* conn, const QueueItemPtr& qi, const string& remoteIp, const string& cipherName) noexcept;
		
		void getParams(StringMap& params) const;
		int64_t getSecondsLeft(bool wholeFile = false) const;
		
		~Download();
		
		/** @return Target filename without path. */
		string getTargetFileName() const
		{
			return Util::getFileName(getPath());
		}
		
		/** @internal */
		const string getDownloadTarget() const
		{
			const auto tmpTarget = getTempTarget();
			if (tmpTarget.empty())
				return getPath();
			else
				return tmpTarget;
		}
		
		/** @internal */		
		TigerTree& getTigerTree() { return tigerTree; }
		const TigerTree& getTigerTree() const { return tigerTree; }
		string& getPFS() { return pfs; }
		
		/** @internal */
		void getCommand(AdcCommand& cmd, bool zlib) const;
		
		string getTempTarget() const;

#ifdef FLYLINKDC_USE_DROP_SLOW
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

		GETSET(bool, treeValid, TreeValid);
		GETSET(string, reason, Reason);

		void resetDownloadFile()
		{
			safe_delete(downloadFile);
		}

		int64_t getDownloadedBytes() const;
		void updateSpeed(uint64_t currentTick);

	private:
		OutputStream* downloadFile;
		const QueueItemPtr qi;
		TigerTree tigerTree;
		string pfs;

	public:
		struct ErrorInfo
		{
			int error;
			Type type;
			string target;
			int64_t size;
		};		
};

typedef std::shared_ptr<Download> DownloadPtr;

#endif /*DOWNLOAD_H_*/

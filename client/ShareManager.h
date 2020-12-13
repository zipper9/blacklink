/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef SHARE_MANAGER_H_
#define SHARE_MANAGER_H_

#include "File.h"
#include "SharedFile.h"
#include "Singleton.h"
#include "HashManagerListener.h"
#include "QueueManagerListener.h"
#include "SettingsManager.h"
#include "TimerManager.h"
#include "HashValue.h"
#include "SearchParam.h"
#include "SearchResult.h"
#include "StringSearch.h"
#include "BloomFilter.h"
#include "LruCache.h"
#include <regex>

class OutputStream;
class SimpleXML;
class AdcCommand;

STANDARD_EXCEPTION_ADD_INFO(ShareException);

struct AdcSearchParam
{
	explicit AdcSearchParam(const StringList& params, unsigned maxResults) noexcept;
			
	bool isExcluded(const string& str) const noexcept;
	bool hasExt(const string& name) noexcept;
	string getDescription() const noexcept;

	StringSearch::List include;
	StringSearch::List exclude;
	StringList exts;
	StringList noExts;
			
	int64_t gt;
	int64_t lt;
			
	TTHValue root;
	bool hasRoot;
	bool isDirectory;
	string token;
	unsigned maxResults;

	string cacheKey;
};

class ShareManager :
	public Singleton<ShareManager>,
	private HashManagerListener,
	private TimerManagerListener,
	private SettingsManagerListener,
	private Thread
{
	public:
		friend class Singleton<ShareManager>;
		friend class ShareLoader;

		enum
		{
			STATE_IDLE,
			STATE_SCANNING_DIRS,
			STATE_HASHING_FILES,
			STATE_CREATING_FILELIST
		};
		
		struct SharedDirInfo
		{
			bool isExcluded;
			string virtualPath;
			string realPath;
			int64_t size; // -1 if unknown
			SharedDirInfo(const string& virtualPath, const string& realPath, int64_t size) :
				isExcluded(false), virtualPath(virtualPath), realPath(realPath), size(size) {}
			SharedDirInfo(const string& excludedPath) :
				isExcluded(true), realPath(excludedPath), size(-1) {}
		};
		
		void addDirectory(const string& realPath, const string &virtualName);
		void removeDirectory(const string& realPath);
		void renameDirectory(const string& realPath, const string& virtualName);
		bool isDirectoryShared(const string& path) const noexcept;
		bool removeExcludeFolder(const string &path);
		bool addExcludeFolder(const string &path);
		void addFile(const string& path, const TTHValue& root);
		void getDirectories(vector<SharedDirInfo>& res) const noexcept;
		bool changed() const noexcept;
		void shutdown();
		bool isRefreshing() const noexcept;
		int getState() const noexcept;

		size_t getSharedTTHCount() const noexcept;
		size_t getSharedFiles() const { return totalFiles; }
		int64_t getSharedSize() const { return totalSize; }
		
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		string getFilePath(const string& virtualPath, bool isHidingShare) const;
		MemoryInputStream* generatePartialList(const string& dir, bool recurse, bool isHidingShare) const;
#else
		string getFilePath(const string& virtualPath) const;
		MemoryInputStream* generatePartialList(const string& dir, bool recurse) const;
#endif
		string getFilePathByTTH(const TTHValue& tth) const;
		MemoryInputStream* getTreeByTTH(const TTHValue& tth) const noexcept;
		MemoryInputStream* getTree(const string& virtualFile) const noexcept;
		static MemoryInputStream* getTreeFromStore(const TTHValue& tth) noexcept;

		static string validateVirtual(const string& virt) noexcept;
		bool isTTHShared(const TTHValue& tth) const noexcept;
		bool getFilePath(const TTHValue& tth, string& path) const noexcept;
		bool getFileInfo(const TTHValue& tth, string& path, int64_t& size) const noexcept;
		bool getFileInfo(const TTHValue& tth, int64_t& size) const noexcept;
		bool getFileInfo(AdcCommand& cmd, const string& filename) const noexcept;
		bool findByRealPath(const string& realPath, TTHValue* outTTH, string* outFilename, int64_t* outSize) const noexcept;
		
		void incHits() { ++hits; }
		void setHits(size_t value) { hits = value; }
		size_t getHits() const { return hits; }

		void getHashBloom(ByteVector& v, size_t k, size_t m, size_t h) const noexcept;
		void load(SimpleXML& xml);
		static string getEmptyBZXmlFile() { return Util::getConfigPath() + "EmptyFiles.xml.bz2"; }
		static string getDefaultBZXmlFile() { return Util::getConfigPath() + "files.xml.bz2"; }
		string getBZXmlFile() const noexcept;
		void saveShareList(SimpleXML& xml) const;

		// Search
		bool searchTTH(const TTHValue& tth, vector<SearchResultCore>& results, const Client* client) noexcept;
		void search(vector<SearchResultCore>& results, const NmdcSearchParam& sp, const Client* client) noexcept;
		void search(vector<SearchResultCore>& results, AdcSearchParam& sp) noexcept;

		bool refreshShare();
		bool refreshShareIfChanged();
		void generateFileList();

	private:
		struct ShareListItem
		{
			BaseDirItem realPath;
			SharedDir* dir;
			int64_t version;
		};

		struct TTHMapItem
		{
			const SharedDir* dir;
			SharedFilePtr file;
		};

		struct FileToHash
		{
			SharedFilePtr file;
			string path;
		};

		typedef vector<ShareListItem> ShareList;
		std::unique_ptr<RWLock> csShare;
		ShareList shares;
		StringList notShared;
		std::atomic<int64_t> totalSize;
		std::atomic<size_t> totalFiles;
		size_t fileCounter;
		bool shareListChanged;
		int64_t versionCounter;

		boost::unordered_map<TTHValue, TTHMapItem> tthIndex;
		BloomFilter<5> bloom;
		
		size_t hits;

		std::atomic_bool doingScanDirs;
		std::atomic_bool doingHashFiles;
		std::atomic_bool doingCreateFileList;
		std::atomic_bool stopScanning;
		std::atomic_bool finishedScanDirs;
		StringList newNotShared;
		boost::unordered_map<TTHValue, TTHMapItem> tthIndexNew;
		BloomFilter<5> bloomNew;
		bool hasRemoved, hasAdded;
		int64_t nextFileID;
		std::atomic<int64_t> maxSharedFileID;
		std::atomic<int64_t> maxHashedFileID;
		vector<FileToHash> filesToHash;
		bool optionShareHidden, optionShareSystem, optionShareVirtual;
		mutable bool optionIncludeHit, optionIncludeTimestamp;
		
		std::atomic_bool stopLoading; // REMOVE
		TTHValue xmlListRoot[2];
		int64_t xmlListLen[2];
		std::atomic<uint64_t> tickUpdateList;
		std::atomic<uint64_t> tickRefresh;
		std::atomic<uint64_t> tickRestoreFileList;
		uint64_t tickLastRefresh;
		unsigned autoRefreshTime;
		
		unsigned tempFileCount;
		string tempBZXmlFile;
		string tempShareDataFile;
		mutable CriticalSection csTempBZXmlFile;
		
		std::regex reSkipList;
		bool hasSkipList;
		mutable FastCriticalSection csSkipList;

		struct CacheItem
		{
			string key;
			vector<SearchResultCore> results;
			CacheItem* next;
		};

		static const size_t SEARCH_CACHE_SIZE = 200;
		LruCache<CacheItem, string> searchCache;
		CriticalSection csSearchCache;

		ShareManager();
		~ShareManager();

		ShareList::const_iterator getByVirtualL(const string& virtualName) const noexcept;
		ShareList::const_iterator getByRealL(const string& realName) const noexcept;
		bool hasShareL(const string& virtualName, const string& realName, bool& foundVirtual) const noexcept;
		void loadShareList(SimpleXML& xml);
		void loadShareData(File& file);
		void loadSharedFile(SharedDir* current, const string& filename, int64_t size, const TTHValue& tth, uint64_t timestamp, uint64_t timeShared, unsigned hit) noexcept;
		void loadSharedDir(SharedDir* &current, const string& filename) noexcept;
		bool addExcludeFolderL(const string& path) noexcept;

		static void writeShareDataDirStart(OutputStream* os, const SharedDir* dir, uint8_t tempBuf[]);
		static void writeShareDataDirEnd(OutputStream* os);
		static void writeShareDataFile(OutputStream* os, const SharedFilePtr& file, uint8_t tempBuf[]);
		void writeDataL(const SharedDir* dir, OutputStream& xmlFile, OutputStream* shareDataFile, string& indent, string& tmp, uint8_t tempBuf[], bool fullList) const;
		void writeFilesDataL(const SharedDir* dir, OutputStream& xmlFile, OutputStream* shareDataFile, string& indent, string& tmp, uint8_t tempBuf[]) const;
		bool generateFileList(uint64_t tick);

		string getADCPathL(const SharedDir* dir) const noexcept;
		string getNMDCPathL(const SharedDir* dir) const noexcept;
		string getFilePathL(const SharedDir* dir) const noexcept;
		bool parseVirtualPathL(const string& virtualPath, const SharedDir* &dir, string& filename) const noexcept;
		bool parseVirtualPathL(const string& virtualPath, const SharedDir* &dir, SharedFilePtr& file) const noexcept;
		bool findByRealPathL(const string& pathLower, SharedDir* &dir, string& filename) const noexcept;
		bool findByRealPathL(const string& pathLower, SharedDir* &dir, SharedFilePtr& file) const noexcept;
		
		void searchL(const SharedDir* dir, vector<SearchResultCore>& results, const StringSearch::List& ssl, const SearchParamBase& sp) noexcept;
		void searchL(const SharedDir* dir, vector<SearchResultCore>& results, AdcSearchParam& sp, const StringSearch::List* replaceInclude) noexcept;

		void scanDirs();
		void scanDir(SharedDir* dir, const string& path);
		bool isDirectoryExcludedL(const string& path) const noexcept;
		void updateIndexDirL(const SharedDir* dir) noexcept; 
		void updateBloomDirL(const SharedDir* dir) noexcept;
		void updateSharedSizeL() noexcept;

		bool isInSkipList(const string& lowerName) const;
		void rebuildSkipList();

		// HashManagerListener
		virtual void on(FileHashed, int64_t fileID, const SharedFilePtr& file, const string& fileName, const TTHValue& root, int64_t size) noexcept override;
		virtual void on(HashingError, int64_t fileID, const SharedFilePtr& file, const string& fileName) noexcept override;
		virtual void on(HashingAborted) noexcept override;

		// TimerManagerListener
		virtual void on(Second, uint64_t tick) noexcept override;

		// SettingsManagerListener
		virtual void on(Repaint) noexcept override;

		// Thread
		virtual int run() override { scanDirs(); return 0; }
};

#endif // SHARE_MANAGER_H_

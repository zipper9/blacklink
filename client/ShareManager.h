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
#include "SettingsManager.h"
#include "TimerManager.h"
#include "HashValue.h"
#include "SearchParam.h"
#include "SearchResult.h"
#include "StringSearch.h"
#include "Streams.h"
#include "BloomFilter.h"
#include "LruCache.h"
#include <regex>

class OutputStream;
class SimpleXML;
class AdcCommand;

STANDARD_EXCEPTION_ADD_INFO(ShareException);

struct AdcSearchParam
{
	explicit AdcSearchParam(const StringList& params, unsigned maxResults, const CID& shareGroup) noexcept;
			
	bool isExcluded(const string& strLower) const noexcept;
	bool hasExt(const string& name) noexcept;
	string getDescription() const noexcept;

	StringSearch::List include;
	StringSearch::List exclude;
	StringList exts;
	StringList noExts;
			
	int64_t gt;
	int64_t lt;
			
	TTHValue root;
	const CID shareGroup;
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

		struct ShareGroupInfo
		{
			CID id;
			string name;
		};

		void addDirectory(const string& realPath, const string &virtualName);
		void removeDirectory(const string& realPath);
		void renameDirectory(const string& realPath, const string& virtualName);
		bool isDirectoryShared(const string& path) const noexcept;
		bool removeExcludeFolder(const string &path) noexcept;
		bool addExcludeFolder(const string &path);
		void addShareGroup(const string& name, const list<string>& shareList, CID& outId);
		void removeShareGroup(const CID& id) noexcept;
		void updateShareGroup(const CID& id, const string& name, const list<string>& shareList);
		void addFile(const string& path, const TTHValue& root);
		void getDirectories(vector<SharedDirInfo>& res) const noexcept;
		void getShareGroups(vector<ShareGroupInfo>& res) const noexcept;
		bool getShareGroupDirectories(const CID& id, boost::unordered_set<string>& dirs) const noexcept;
		bool getShareGroupDirectories(const CID& id, list<string>& dirs) const noexcept;
		void shutdown();

		bool changed() const noexcept;
		bool isRefreshing() const noexcept;
		int getState() const noexcept;
		int64_t getShareListVersion() const noexcept;
		void getScanProgress(int64_t result[]) const noexcept;
		uint64_t getLastRefreshTime() const noexcept { return timeLastRefresh; }

		size_t getSharedTTHCount() const noexcept;
		size_t getTotalSharedFiles() const noexcept { return totalFiles; }
		int64_t getTotalSharedSize() const noexcept { return totalSize; }
		bool getShareGroupInfo(const CID& id, int64_t& size, int64_t& files) const noexcept;
		bool getShareGroupName(const CID& id, string& name) const noexcept;
		
		string getFileByPath(const string& virtualPath, bool hideShare, const CID& shareGroup, int64_t& xmlSize) const;
		MemoryInputStream* generatePartialList(const string& dir, bool recurse, bool hideShare, const CID& shareGroup) const;
		string getFileByTTH(const TTHValue& tth, bool hideShare, const CID& shareGroup) const;
		MemoryInputStream* getTreeByTTH(const TTHValue& tth) const noexcept;
		MemoryInputStream* getTree(const string& virtualFile, const CID& shareGroup) const noexcept;
		static MemoryInputStream* getTreeFromStore(const TTHValue& tth) noexcept;

		static string validateVirtual(const string& virt) noexcept;
		bool isTTHShared(const TTHValue& tth) const noexcept;
		bool getFilePath(const TTHValue& tth, string& path, const CID& shareGroup) const noexcept;
		bool getFileInfo(const TTHValue& tth, string& path, int64_t& size) const noexcept;
		bool getFileInfo(const TTHValue& tth, string& path) const noexcept;
		bool getFileInfo(const TTHValue& tth, int64_t& size) const noexcept;
		bool getFileInfo(AdcCommand& cmd, const string& filename, bool hideShare, const CID& shareGroup) const noexcept;
		bool findByRealPath(const string& realPath, TTHValue* outTTH, string* outFilename, int64_t* outSize) const noexcept;
		
		void incHits() { ++hits; }
		void setHits(size_t value) { hits = value; }
		size_t getHits() const { return hits; }

		void getHashBloom(ByteVector& v, size_t k, size_t m, size_t h) const noexcept;
		void load(SimpleXML& xml);
		void init();
		static string getEmptyBZXmlFile() { return Util::getConfigPath() + "EmptyFiles.xml.bz2"; }
		string getBZXmlFile(const CID& id, int64_t& xmlSize) const noexcept;
		void saveShareList(SimpleXML& xml) const;

		// Search
		bool searchTTH(const TTHValue& tth, vector<SearchResultCore>& results, const Client* client, const CID& shareGroup) noexcept;
		void search(vector<SearchResultCore>& results, const NmdcSearchParam& sp, const Client* client) noexcept;
		void search(vector<SearchResultCore>& results, AdcSearchParam& sp) noexcept;
#ifdef _DEBUG
		bool matchBloom(const string& s) const noexcept;
		void getBloomInfo(size_t& size, size_t& used) const noexcept;
#endif

		bool refreshShare();
		bool refreshShareIfChanged();
		void generateFileList() noexcept;

	private:
		typedef BloomFilter<5> Bloom;

		enum
		{
			FILE_ATTR_FILES_XML,
			FILE_ATTR_FILES_BZ_XML,
			FILE_ATTR_EMPTY_FILES_XML,
			FILE_ATTR_EMPTY_FILES_BZ_XML,
			MAX_FILE_ATTR
		};

		enum
		{
			REFRESH_MODE_NONE,
			REFRESH_MODE_FULL,
			REFRESH_MODE_FILE_LIST
		};

		struct ShareListItem
		{
			BaseDirItem realPath;
			SharedDir* dir;
			int64_t version;
			int64_t totalFiles;
			uint16_t flags;
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

		struct FileAttr
		{
			TTHValue root;
			int64_t size;
		};

		struct ShareGroup
		{
			CID id;
			string name;
			TTHValue hash;
			list<BaseDirItem> shares;
			FileAttr attrComp;
			FileAttr attrUncomp;
			int64_t totalSize;
			int64_t totalFiles;
			string tempXmlFile;

			ShareGroup()
			{
				attrComp.size = attrUncomp.size = -1;
				totalSize = 0;
				totalFiles = 0;
			}

			bool hasShare(const ShareListItem& share) const;
		};

		typedef vector<ShareListItem> ShareList;
		std::unique_ptr<RWLock> csShare;
		ShareList shares;
		StringList notShared;
		int64_t shareListVersion;
		std::atomic<int64_t> totalSize;
		std::atomic<size_t> totalFiles;
		size_t fileCounter;
		bool shareListChanged;
		bool fileListChanged;
		int autoRefreshMode;
		int64_t versionCounter;
		boost::unordered_map<CID, ShareGroup> shareGroups;

		FileAttr fileAttr[MAX_FILE_ATTR];

		boost::unordered_multimap<TTHValue, TTHMapItem> tthIndex;
		Bloom bloom;
		
		size_t hits;

		std::atomic_bool doingScanDirs;
		std::atomic_bool doingHashFiles;
		std::atomic_bool doingCreateFileList;
		std::atomic_bool stopScanning;
		std::atomic_bool finishedScanDirs;
		StringList newNotShared;
		boost::unordered_multimap<TTHValue, TTHMapItem> tthIndexNew;
		Bloom bloomNew;
		unsigned scanShareFlags;
		unsigned scanAllFlags;
		int64_t nextFileID;
		std::atomic<int64_t> maxSharedFileID;
		std::atomic<int64_t> maxHashedFileID;
		std::atomic<int64_t> scanProgress[2];
		vector<FileToHash> filesToHash;
		bool optionShareHidden, optionShareSystem, optionShareVirtual;
		mutable bool optionIncludeUploadCount, optionIncludeTimestamp;
		HashDatabaseConnection* hashDb;

#if 0
		std::atomic_bool stopLoading = false;
#endif
		std::atomic<uint64_t> tickUpdateList;
		std::atomic<uint64_t> tickRefresh;
		std::atomic<uint64_t> tickRestoreFileList;
		uint64_t tickLastRefresh;
		std::atomic<uint64_t> timeLastRefresh;
		unsigned autoRefreshTime;
		
		unsigned tempFileCount;
		string tempShareDataFile;
		
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
		void loadSharedFile(SharedDir* current, const string& filename, int64_t size, const TTHValue& tth, uint64_t timestamp, uint64_t timeShared) noexcept;
		void loadSharedDir(SharedDir* &current, const string& filename) noexcept;
		bool addExcludeFolderL(const string& path) noexcept;
		void addShareGroupL(const string& name, const CID& id, const list<string>& shares, const FileAttr* attr);
		static void removeShareGroupFiles(const string& path) noexcept;
		void removeOldShareGroupFiles() noexcept;

		static void writeShareDataDirStart(OutputStream* os, const SharedDir* dir, uint8_t tempBuf[]);
		static void writeShareDataDirEnd(OutputStream* os);
		static void writeShareDataFile(OutputStream* os, const SharedFilePtr& file, uint8_t tempBuf[]);
		void writeShareDataL(const SharedDir* dir, OutputStream* shareDataFile, uint8_t tempBuf[]) const;
		void writeXmlL(const SharedDir* dir, OutputStream& xmlFile, string& indent, string& tmp, int mode) const;
		void writeXmlFilesL(const SharedDir* dir, OutputStream& xmlFile, string& indent, string& tmp) const;
		bool renameXmlFiles() noexcept;
		bool getXmlFileInfo(const CID& id, bool compressed, TTHValue& tth, int64_t& size) const noexcept;
		bool writeShareGroupXml(const CID& id);
		bool writeEmptyFileList(const string& path) noexcept;
		bool generateFileList(uint64_t tick) noexcept;

		string getADCPathL(const SharedDir* dir) const noexcept;
		string getNMDCPathL(const SharedDir* dir) const noexcept;
		string getNMDCPathL(const SharedDir* dir, const SharedDir* &topDir) const noexcept;
		string getFilePathL(const SharedDir* dir, const ShareListItem* &share) const noexcept;
		bool parseVirtualPathL(const string& virtualPath, const SharedDir* &dir, string& filename, const ShareGroup& sg) const noexcept;
		bool parseVirtualPathL(const string& virtualPath, const SharedDir* &dir, SharedFilePtr& file, const ShareGroup& sg) const noexcept;
		bool findByRealPathL(const string& pathLower, SharedDir* &dir, string& filename) const noexcept;
		bool findByRealPathL(const string& pathLower, SharedDir* &dir, SharedFilePtr& file) const noexcept;
		
		void searchL(const SharedDir* dir, vector<SearchResultCore>& results, const StringSearch::List& ssl, const SearchParamBase& sp) noexcept;
		void searchL(const SharedDir* dir, vector<SearchResultCore>& results, AdcSearchParam& sp, const StringSearch::List* replaceInclude) noexcept;

		void scanDirs();
		void scanDir(SharedDir* dir, const string& path);
		bool isDirectoryExcludedL(const string& path) const noexcept;
		void updateIndexDirL(const SharedDir* dir) noexcept; 
		void updateBloomDirL(const SharedDir* dir) noexcept;
		void updateBloomL() noexcept;
		void updateSharedSizeL() noexcept;

		void initDefaultShareGroupL() noexcept;
		bool updateShareGroupHashL(ShareGroup& sg);

		bool isInSkipList(const string& lowerName) const;
		void rebuildSkipList();

		bool readFileAttr(const string& path, FileAttr attr[MAX_FILE_ATTR], CID& cid) noexcept;
		bool writeFileAttr(const string& path, const FileAttr attr[MAX_FILE_ATTR]) const noexcept;

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

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


#ifndef DIRECTORY_LISTING_H_
#define DIRECTORY_LISTING_H_

#include "SimpleXML.h"
#include "QueueItem.h"
#include "MediaInfoUtil.h"
#include <atomic>
#include <regex>

STANDARD_EXCEPTION(AbortException);

class DirectoryListing
{
	public:
		class ProgressNotif
		{
			public:
				virtual void notify(int progress) {};
		};

		class Directory;
		class SearchQuery;

		enum
		{
			FLAG_QUEUED         = 1 << 0,
			FLAG_SHARED         = 1 << 1,  // files only
			FLAG_DOWNLOADED     = 1 << 2,
			FLAG_CANCELED       = 1 << 3,
			FLAG_HAS_QUEUED     = 1 << 4,  // dirs only
			FLAG_HAS_SHARED     = 1 << 5,
			FLAG_HAS_DOWNLOADED = 1 << 6,
			FLAG_HAS_CANCELED   = 1 << 7,
			FLAG_HAS_OTHER      = 1 << 8,
			FLAG_FOUND          = 1 << 9,  // files and dirs
			FLAG_HAS_FOUND      = 1 << 10, // dirs only
			FLAG_DIR_TIMESTAMP  = 1 << 11, // temporary flag used during parsing
			FLAG_DCLST_SELF     = 1 << 12, // files
			FILE_STATUS_FLAGS   = FLAG_QUEUED | FLAG_SHARED | FLAG_DOWNLOADED | FLAG_CANCELED,
			DIR_STATUS_FLAGS    = FLAG_HAS_QUEUED | FLAG_HAS_SHARED | FLAG_HAS_DOWNLOADED | FLAG_HAS_CANCELED | FLAG_HAS_OTHER
		};

		enum
		{
			SCAN_OPTION_SHARED          = 1 << 0,
			SCAN_OPTION_DOWNLOADED      = 1 << 1,
			SCAN_OPTION_CANCELED        = 1 << 2,
			SCAN_OPTION_SHOW_MY_UPLOADS = 1 << 3
		};

		typedef boost::unordered_map<TTHValue, int64_t> TTHMap;

		class File : public BaseFlags<uint16_t>
		{
			public:
				typedef vector<File*> List;
				
				File(Directory* dir, const string& name, int64_t size, const TTHValue& tth, uint32_t uploadCount, int64_t ts, const MediaInfoUtil::Info *media) noexcept :
					name(name), size(size), parent(dir), tthRoot(tth),
					uploadCount(uploadCount), ts(ts), userData(nullptr)
				{
					if (media) this->media = std::make_shared<MediaInfoUtil::Info>(*media);
				}

				File(const File& rhs) :
					name(rhs.name), path(rhs.path), size(rhs.size), parent(rhs.parent), tthRoot(rhs.tthRoot),
					uploadCount(rhs.uploadCount), ts(rhs.ts), media(rhs.media), userData(nullptr)
				{
				}

				virtual ~File() {}

				bool match(const SearchQuery &sq) const;

				const string& getName() const { return name; }	
				Directory* getParent() { return parent; }
				void setParent(Directory* dir) { parent = dir; }
				const Directory* getParent() const { return parent; }
				const MediaInfoUtil::Info* getMedia() const { return media.get(); }
				virtual bool getAdls() const { return false; }
				virtual File* clone() const { return new File(*this); }

				GETSET(int64_t, size, Size);
				GETSET(TTHValue, tthRoot, TTH);
				GETSET(uint32_t, uploadCount, UploadCount);
				GETSET(int64_t, ts, TS);
				GETSET(void*, userData, UserData);

				const string& getPath() const { return path; }
				void setPath(const string& path) { this->path = path; }
				
				File& operator= (const File &) = delete;

			private:
				string name;
				string path;
				Directory *parent;
				std::shared_ptr<MediaInfoUtil::Info> media;
		};

		typedef boost::unordered_map<TTHValue, list<File*>> TTHToFileMap;

		struct SpliceTreeResult
		{
			void* parentUserData;
			Directory* firstItem;
			bool insertParent;
		};

		bool spliceTree(DirectoryListing& tree, SpliceTreeResult& sr);

		class Directory : public BaseFlags<uint16_t>
		{
			public:
				typedef vector<Directory*> List;

				List directories;				
				File::List files;

				Directory(Directory* parent, const string& name, bool complete)
					: parent(parent), name(name), complete(complete), userData(nullptr),
					totalFileCount(0), totalDirCount(0), totalSize(0), maxTS(0), totalUploadCount(0),
					minBitrate(0xFFFF), maxBitrate(0)
				{
				}
				
				virtual ~Directory();
				
				void filterList(const TTHMap& l);
				void clearMatches();
				const File* findFileByHash(const TTHValue& tth) const;
				void getHashList(TTHMap& l) const;
				void findDuplicates(TTHToFileMap& m, int64_t minSize) const;
				void matchTTHSet(const TTHMap& l);
				bool match(const SearchQuery &sq) const;
				const string& getName() const { return name; }
				Directory* getParent() { return parent; }
				const Directory* getParent() const { return parent; }
				virtual bool getAdls() const { return false; }
				virtual Directory* clone(Directory* parent) const { return new Directory(parent, name, true); }

				GETSET(bool, complete, Complete);
				GETSET(void*, userData, UserData);

				size_t getTotalFileCount() const { return totalFileCount; }
				size_t getTotalFolderCount() const { return totalDirCount; }
				int64_t getTotalSize() const { return totalSize; }
				uint32_t getTotalUploadCount() const { return totalUploadCount; }
				int64_t getMaxTS() const { return maxTS; }
				uint16_t getMinBirate() const { return minBitrate; }
				uint16_t getMaxBirate() const { return maxBitrate; }

				static void updateInfo(DirectoryListing::Directory* dir);
				static void updateFlags(DirectoryListing::Directory* dir);
				void addFile(DirectoryListing::File *f);

				Directory(const Directory &) = delete;
				Directory& operator= (const Directory &) = delete;

			protected:
				Directory *parent;
				string name;
				size_t totalFileCount;
				size_t totalDirCount;
				int64_t totalSize;
				int64_t maxTS;
				uint32_t totalUploadCount;
				uint16_t minBitrate;
				uint16_t maxBitrate;

				void updateSubDirs(MaskType& updatedFlags);
				void updateFiles(MaskType& updatedFlags);
				bool updateFlags();
		
				friend bool DirectoryListing::spliceTree(DirectoryListing& tree, SpliceTreeResult& sr);
				friend class ListLoader;
		};
		
		class AdlDirectory : public Directory
		{
			public:
				AdlDirectory(const string& fullPath, Directory* parent, const string& name) :
					Directory(parent, name, true), fullPath(fullPath) {}
				
				bool getAdls() const override { return true; }
				AdlDirectory* clone(Directory* parent) const override { return new AdlDirectory(fullPath, parent, getName()); }
				GETSET(string, fullPath, FullPath);

				using Directory::updateFlags;
		};

		class AdlFile : public File
		{
			public:
				AdlFile(const string& fullPath, const File& rhs) :
					File(rhs), fullPath(fullPath) {}

				bool getAdls() const override { return true; }
				AdlFile* clone() const override { return new AdlFile(fullPath, *this); }
				GETSET(string, fullPath, FullPath);
		};

		class SearchQuery
		{			
			public:
				enum
				{
					FLAG_STRING        = 0x001,
					FLAG_WSTRING       = 0x002,
					FLAG_REGEX         = 0x004,
					FLAG_MATCH_CASE    = 0x008,
					FLAG_TYPE          = 0x010,
					FLAG_SIZE          = 0x020,
					FLAG_TIME_SHARED   = 0x040,
					FLAG_SKIP_OWNED    = 0x080,
					FLAG_SKIP_CANCELED = 0x100,
					FLAG_SKIP_EMPTY    = 0x200
				};

				SearchQuery(): flags(0) {}

				unsigned flags;
				string text;
				wstring wtext;
				std::regex re;
				int type;
				int64_t minSize, maxSize;
				int64_t minSharedTime;
				TTHValue tth;
				mutable wstring wbuf;
		};

		// Internal class
		struct CopiedDir
		{
			const Directory *src;
			Directory *copy;
			
			CopiedDir() {}
			CopiedDir(const Directory *src, Directory *copy): src(src), copy(copy) {}
		};
		
		class SearchContext
		{
			public:
				enum
				{
					FOUND_NOTHING,
					FOUND_FILE,
					FOUND_DIR
				};				

				SearchContext();
				bool match(const SearchQuery &sq, Directory *root, DirectoryListing *dest, vector<const Directory*> &pathCache);
				bool next();
				bool prev();
				bool goToFirstFound(const Directory *root);
				
				int getWhatFound() const { return whatFound; }
				const Directory* getDirectory() const { return dir; }
				const File* getFile() const { return file; }
				const vector<int>& getDirIndex() const { return dirIndex; }
				bool setFound(const Directory *dir);
				bool setFound(const File *file);
				void clear();
				const size_t* getCount() const { return matched; }

			private:
				int fileIndex;
				int whatFound;
				const Directory *dir;
				const File *file;
				vector<int> dirIndex;
				vector<CopiedDir> copiedPath;
				size_t matched[2];

				bool makeIndexForFound(const Directory *dir);
				void createCopiedPath(const Directory *dir, vector<const Directory*> &pathCache);
		};
		
		DirectoryListing(std::atomic_bool& abortFlag, bool createRoot = true, const DirectoryListing* src = nullptr);
		~DirectoryListing();
		
		void loadFile(const string& fileName, ProgressNotif *progressNotif, bool ownList);
		
		void loadXML(const std::string&, ProgressNotif *progressNotif, bool ownList);
		void loadXML(InputStream& xml, ProgressNotif *progressNotif, bool ownList);
		
		void download(Directory* dir, const string& target, QueueItem::Priority prio, bool& getConnFlag);
		void download(File* file, const string& target, bool view, QueueItem::Priority prio, bool isDclst, bool& getConnFlag) noexcept;
		
		string getPath(const Directory* d) const;
		string getPath(const File* f) const
		{
			return getPath(f->getParent());
		}
		
		const Directory* getRoot() const { return root; }
		Directory* getRoot() { return root; }
		
		static UserPtr getUserFromFilename(const string& fileName);
		
		const UserPtr& getUser() const
		{
			return hintedUser.user;
		}

		void buildTTHSet();
		const TTHMap* getTTHSet() const { return tthSet; }
		void clearTTHSet();
		void findDuplicates(TTHToFileMap& result, int64_t minSize);
		void matchTTHSet(const TTHMap& l);

		bool isOwnList() const { return ownList; }
		const string& getBasePath() const { return basePath; }

		Directory* findDirPath(const string& path) const;
		const Directory* findDirPathNoCase(const string& path) const;

		bool isAborted() const { return aborted; }
		bool hasTimestamps() const { return hasTimestampsFlag; }

		static void markAsFound(File* file) noexcept;
		void addDclstSelf(const string& filePath, std::atomic_bool& stopFlag);
		void getFileParams(const File* f, StringMap& ucParams) const noexcept;
		void getDirectoryParams(const Directory* d, StringMap& ucParams) const noexcept;

		GETSET(HintedUser, hintedUser, HintedUser);
		GETSET(bool, includeSelf, IncludeSelf);
		GETSET(int, scanOptions, ScanOptions);

	private:
		friend class ListLoader;
		
		Directory* root;
		TTHMap* tthSet;
		bool ownList;
		bool incomplete;
		string basePath;
		std::atomic_bool& abortFlag;
		bool aborted;
		bool hasTimestampsFlag;
};

#endif // !defined(DIRECTORY_LISTING_H_)

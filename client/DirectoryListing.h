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


#ifndef DCPLUSPLUS_DCPP_DIRECTORY_LISTING_H
#define DCPLUSPLUS_DCPP_DIRECTORY_LISTING_H

#include "SimpleXML.h"
#include "QueueItem.h"
#include "UserInfoBase.h"
#include <atomic>

class ListLoader;
class DirectoryListingFrame;
STANDARD_EXCEPTION(AbortException);

class DirectoryListing : public UserInfoBase
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
			FLAG_SHARED         = 1 << 1, // files only
			FLAG_DOWNLOADED     = 1 << 2,
			FLAG_CANCELED       = 1 << 3,
			FLAG_HAS_QUEUED     = 1 << 4, // dirs only
			FLAG_HAS_SHARED     = 1 << 5,
			FLAG_HAS_DOWNLOADED = 1 << 6,
			FLAG_HAS_CANCELED   = 1 << 7,
			FLAG_HAS_OTHER      = 1 << 8,
			FLAG_FOUND          = 1 << 9, // files and dirs
			FLAG_HAS_FOUND      = 1 << 10  // dirs only
		};
		
		struct MediaInfo
		{
			uint16_t width;
			uint16_t height;
			uint16_t bitrate;
			string audio;
			string video;

			uint32_t getSize() const
			{
				return (uint32_t) width << 16 | height;
			}
		};

		typedef boost::unordered_map<TTHValue, int64_t> TTHMap;

		class File : public Flags
		{
			public:
				typedef vector<File*> List;
				
				File(Directory* dir, const string& name, int64_t size, const TTHValue& tth, uint32_t hit, int64_t ts, const MediaInfo *media) noexcept :
					name(name), size(size), parent(dir), tthRoot(tth),
					hit(hit), ts(ts), adls(false), userData(nullptr)
				{
					if (media) this->media = std::make_shared<MediaInfo>(*media);
				}

				File(const File& rhs) :
					name(rhs.name), path(rhs.path), size(rhs.size), parent(rhs.parent), tthRoot(rhs.tthRoot),
					hit(rhs.hit), ts(rhs.ts), media(rhs.media), adls(rhs.adls), userData(nullptr)
				{
				}

				~File()
				{
				}

				bool match(const SearchQuery &sq) const;

				const string& getName() const { return name; }	
				Directory* getParent() { return parent; }
				const Directory* getParent() const { return parent; }
				const MediaInfo* getMedia() const { return media.get(); }

				GETSET(int64_t, size, Size);
				GETSET(TTHValue, tthRoot, TTH);
				GETSET(uint32_t, hit, Hit);
				GETSET(int64_t, ts, TS);
				GETSET(bool, adls, Adls);
				GETSET(void*, userData, UserData);

				const string& getPath() const { return path; }
				void setPath(const string& path) { this->path = path; }
				
				File& operator= (const File &) = delete;

			private:
				string name;
				string path;
				Directory *parent;
				std::shared_ptr<MediaInfo> media;
		};

		typedef boost::unordered_map<TTHValue, list<File*>> TTHToFileMap;

		bool spliceTree(Directory* dest, DirectoryListing& tree);

		class Directory : public Flags
		{
			public:				
				typedef vector<Directory*> List;				
				
				List directories;				
				File::List files;

				Directory(Directory* parent, const string& name, bool adls, bool complete)
					: parent(parent), name(name), adls(adls), complete(complete), userData(nullptr),
					totalFileCount(0), totalDirCount(0), totalSize(0), maxTS(0), totalHits(0),
					minBitrate(0xFFFF), maxBitrate(0)
				{
				}
				
				virtual ~Directory();
				
				void filterList(const TTHMap& l);
				void clearMatches();
				void getHashList(TTHMap& l) const;
				void findDuplicates(TTHToFileMap& m, int64_t minSize) const;
				bool match(const SearchQuery &sq) const;
				const string& getName() const { return name; }
				Directory* getParent() { return parent; }
				const Directory* getParent() const { return parent; }
				bool getAdls() const { return adls; }
				GETSET(bool, complete, Complete);
				GETSET(void*, userData, UserData);

				size_t getTotalFileCount() const { return totalFileCount; }
				size_t getTotalFolderCount() const { return totalDirCount; }
				int64_t getTotalSize() const { return totalSize; }
				uint32_t getTotalHits() const { return totalHits; }
				int64_t getMaxTS() const { return maxTS; }
				uint16_t getMinBirate() const { return minBitrate; }
				uint16_t getMaxBirate() const { return maxBitrate; }

				static void updateInfo(DirectoryListing::Directory* dir);
				void addFile(DirectoryListing::File *f);

				Directory(const Directory &) = delete;
				Directory& operator= (const Directory &) = delete;

			private:
				Directory *parent;
				string name;
				bool adls;
				size_t totalFileCount;
				size_t totalDirCount;
				int64_t totalSize;
				int64_t maxTS;
				uint32_t totalHits;
				uint16_t minBitrate;
				uint16_t maxBitrate;

				void updateSubDirs(Flags::MaskType& updatedFlags);
				void updateFiles(Flags::MaskType& updatedFlags);
		
				friend bool DirectoryListing::spliceTree(Directory* dest, DirectoryListing& tree);
				friend class ListLoader;
		};
		
		class AdlDirectory : public Directory
		{
			public:
				AdlDirectory(const string& fullPath, Directory* parent, const string& name) :
					Directory(parent, name, true, true), fullPath(fullPath) { }
				
				GETSET(string, fullPath, FullPath);
		};
		
		class SearchQuery
		{			
			public:
				enum
				{
					FLAG_STRING         = 1,
					FLAG_WSTRING        = 2,
					FLAG_REGEX          = 4,
					FLAG_MATCH_CASE     = 8,
					FLAG_TYPE           = 16,
					FLAG_SIZE           = 32,
					FLAG_TIME_SHARED    = 64,
					FLAG_ONLY_NEW_FILES = 128
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

			private:
				int fileIndex;
				int whatFound;
				const Directory *dir;
				const File *file;
				vector<int> dirIndex;
				vector<CopiedDir> copiedPath;

				bool makeIndexForFound(const Directory *dir);
				void createCopiedPath(const Directory *dir, vector<const Directory*> &pathCache);
		};
		
		DirectoryListing(std::atomic_bool& abortFlag, bool createRoot = true, const DirectoryListing* src = nullptr);
		~DirectoryListing();
		
		void loadFile(const string& fileName, ProgressNotif *progressNotif, bool ownList);
		
		void loadXML(const std::string&, ProgressNotif *progressNotif, bool ownList);
		void loadXML(InputStream& xml, ProgressNotif *progressNotif, bool ownList);
		
		void download(Directory* dir, const string& target, QueueItem::Priority prio, bool& getConnFlag);
		void download(File* file, const string& target, bool view, QueueItem::Priority prio, bool isDclst, bool& getConnFlag);
		
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

		bool isOwnList() const { return ownList; }
		const string& getBasePath() const { return basePath; }

		Directory* findDirPath(const string& path) const;

		bool isAborted() const { return aborted; }
		bool hasTimestamps() const { return hasTimestampsFlag; }

		GETSET(HintedUser, hintedUser, HintedUser);
		GETSET(bool, includeSelf, IncludeSelf);
	
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

#endif // !defined(DIRECTORY_LISTING_H)

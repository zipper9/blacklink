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

/*
 * Automatic Directory Listing Search
 * Henrik Engström, henrikengstrom at home se
 */

#ifndef ADL_SEARCH_H
#define ADL_SEARCH_H

#include "SettingsManager.h"
#include "StringSearch.h"
#include "DirectoryListing.h"
#include "RWLock.h"
#include <atomic>

class ADLSearch
{
	public:
		ADLSearch();

		// The search string
		string searchString;
		bool isCaseSensitive;
		bool isRegEx;

		// Active search
		bool isActive;

		// Forbidden file
		bool isForbidden;
		string userCommand;

		// Auto Queue Results
		bool isAutoQueue;

		// Search source type
		enum SourceType
		{
			OnlyFile,
			OnlyDirectory,
			FullPath,
			TTH,
			TypeLast
		} sourceType;

		static SourceType stringToSourceType(const string& s);
		static const string& sourceTypeToString(SourceType t);
		static const tstring& sourceTypeToDisplayString(SourceType t);

		// Maximum & minimum file sizes (in bytes).
		// Negative values means do not check.
		int64_t minFileSize;
		int64_t maxFileSize;

		enum SizeType
		{
			SizeBytes,
			SizeKiloBytes,
			SizeMegaBytes,
			SizeGigaBytes
		};

		SizeType typeFileSize;

		static SizeType stringToSizeType(const string& s);
		static const string& sizeTypeToString(SizeType t);

		string destDir;
};

class ADLSearchManager : public Singleton<ADLSearchManager>
{
	public:
		typedef vector<ADLSearch> SearchCollection;

		ADLSearchManager();
		~ADLSearchManager();

		void load() noexcept;
		void save() noexcept;
		void saveOnTimer(uint64_t tick) noexcept;
		void matchListing(DirectoryListing* dl, std::atomic_bool* abortFlag) const noexcept;
		void setDirtyL();
		bool isEmpty() const;

		class LockInstance
		{
			const bool write;
			ADLSearchManager* const am;

		public:
			explicit LockInstance(ADLSearchManager* am, bool write) : am(am), write(write)
			{
				if (write)
					am->csCollection->acquireExclusive();
				else
					am->csCollection->acquireShared();
			}
			~LockInstance()
			{
				if (write)
					am->csCollection->releaseExclusive();
				else
					am->csCollection->releaseShared();
			}
			SearchCollection& getCollection() { return am->collection; }
		};

	private:
		SearchCollection collection;
		mutable std::unique_ptr<RWLock> csCollection;
		bool modified;
		uint64_t nextSaveTime;

		struct SearchContextItem
		{
			bool prepare(const ADLSearch& search, const StringMap& params) noexcept;
			bool matchFile(const string& fullPath, const DirectoryListing::File* file) const noexcept;
			bool matchDirectory(const DirectoryListing::Directory* dir) const noexcept;
			bool matchString(const string& s) const noexcept;

			ADLSearch::SourceType sourceType;
			StringSearch::List stringSearches;
			std::regex re;
			bool isRegEx;
			bool isAutoQueue;
			bool isForbidden;
			TTHValue tth;
			size_t destDirIndex;
			int64_t minFileSize;
			int64_t maxFileSize;
			ADLSearch::SizeType typeFileSize;
			int userCommandId;
		};

		struct DestDir
		{
			string name;
			DirectoryListing::AdlDirectory* dir;
		};

		struct SearchContext
		{
			vector<SearchContextItem> collection;
			vector<DestDir> destDir;
			bool breakOnFirst = false;
			bool wantFullPath = false;
			DirectoryListing* dl = nullptr;
			UserPtr user = nullptr;
			std::atomic_bool* abortFlag = nullptr;
			boost::unordered_set<int> sentCommands;

			SearchContext() {}
			~SearchContext();

			SearchContext(const SearchContext&) = delete;
			SearchContext& operator= (const SearchContext&) = delete;

			void match() noexcept;
			bool matchFile(const DirectoryListing::File* file) noexcept;
			bool matchDirectory(const DirectoryListing::Directory* dir) noexcept;
			void insertResults() noexcept;
		};

		struct CopiedDir
		{
			const DirectoryListing::Directory* src;
			DirectoryListing::AdlDirectory* copy;
		};

		void saveL() const noexcept;
		void prepare(SearchContext& ctx, DirectoryListing* dl) const noexcept;
		static void copyDirectory(DirectoryListing::Directory* adlsDestDir, const DirectoryListing::Directory* src, const DirectoryListing* dl) noexcept;
};

#endif // !defined(ADL_SEARCH_H)

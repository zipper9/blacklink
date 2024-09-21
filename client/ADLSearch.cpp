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

#include "stdinc.h"
#include "ADLSearch.h"
#include "AppPaths.h"
#include "QueueManager.h"
#include "LogManager.h"
#include "SimpleStringTokenizer.h"
#include "ParamExpander.h"
#include "FavoriteManager.h"
#include "ClientManager.h"
#include "TimeUtil.h"
#include "ConfCore.h"

static string getConfigFile()
{
	return Util::getConfigPath() + "ADLSearch.xml";
}

ADLSearch::ADLSearch() :
	isCaseSensitive(false),
	isRegEx(false),
	isActive(true),
	isAutoQueue(false),
	sourceType(OnlyFile),
	minFileSize(-1),
	maxFileSize(-1),
	typeFileSize(SizeBytes),
	isForbidden(false)
{
}

static const string defDestDir("ADLSearch");
static const string strDiscard("discard");

static const string strFilename("Filename");
static const string strDirectory("Directory");
static const string strFullPath("Full Path");
static const string strTTH("TTH");

static const string strKB("kB");
static const string strMB("MB");
static const string strGB("GB");
static const string strB("B");

static const string strADLSearch("ADLSearch");
static const string strSearchGroup("SearchGroup");
static const string strSearch("Search");
static const string strSearchString("SearchString");
static const string strSourceType("SourceType");
static const string strDestDirectory("DestDirectory");
static const string strIsActive("IsActive");
static const string strIsForbidden("IsForbidden");
static const string strUserCommand("UserCommand");
static const string strMaxSize("MaxSize");
static const string strMinSize("MinSize");
static const string strSizeType("SizeType");
static const string strIsAutoQueue("IsAutoQueue");
static const string strIsRegExp("IsRegExp");
static const string strIsCaseSensitive("IsCaseSensitive");
static const string strRegEx("RegEx");

ADLSearch::SourceType ADLSearch::stringToSourceType(const string& s)
{
	if (Text::asciiEqual(s, strFilename))
		return OnlyFile;
	if (Text::asciiEqual(s, strDirectory))
		return OnlyDirectory;
	if (Text::asciiEqual(s, strFullPath))
		return FullPath;
	if (Text::asciiEqual(s, strTTH))
		return TTH;
	return OnlyFile;
}

const string& ADLSearch::sourceTypeToString(SourceType t)
{
	switch (t)
	{
		default:
		case OnlyFile:
			return strFilename;
		case OnlyDirectory:
			return strDirectory;
		case FullPath:
			return strFullPath;
		case TTH:
			return strTTH;
	}
}

const tstring& ADLSearch::sourceTypeToDisplayString(SourceType t)
{
	switch (t)
	{
		default:
		case OnlyFile:
			return TSTRING(ADLS_FILE_NAME);
		case OnlyDirectory:
			return TSTRING(ADLS_DIRECTORY_NAME);
		case FullPath:
			return TSTRING(ADLS_FULL_PATH);
		case TTH:
			return TSTRING(TTH);
	}
}

ADLSearch::SizeType ADLSearch::stringToSizeType(const string& s)
{
	if (stricmp(s, strB) == 0)
		return SizeBytes;
	if (stricmp(s, strKB) == 0)
		return SizeKiloBytes;
	if (stricmp(s, strMB) == 0)
		return SizeMegaBytes;
	if (stricmp(s, strGB) == 0)
		return SizeGigaBytes;
	return SizeBytes;
}

const string& ADLSearch::sizeTypeToString(SizeType t)
{
	switch (t)
	{
		case SizeKiloBytes:
			return strKB;
		case SizeMegaBytes:
			return strMB;
		case SizeGigaBytes:
			return strGB;
		default:
		case SizeBytes:
			return strB;
	}
}

bool ADLSearchManager::SearchContextItem::prepare(const ADLSearch& search, const StringMap& params) noexcept
{
	sourceType = search.sourceType;
	minFileSize = search.minFileSize;
	maxFileSize = search.maxFileSize;
	typeFileSize = search.typeFileSize;
	isAutoQueue = search.isAutoQueue;
	isForbidden = search.isForbidden;
	stringSearches.clear();

	if (!search.userCommand.empty())
		userCommandId = FavoriteManager::getInstance()->findUserCommand(search.userCommand, UserCommand::CONTEXT_FILELIST);
	else
		userCommandId = -1;

	if (search.sourceType == ADLSearch::TTH)
	{
		bool error;
		Util::fromBase32(search.searchString.c_str(), tth.data, TTHValue::BYTES, &error);
		return !error;
	}

	// Replace parameters such as %[nick]
	const string s = Util::formatParams(search.searchString, params, false);

	bool result = true;
	isRegEx = false;
	if (search.isRegEx)
	{
		try
		{
			re = std::regex(s, search.isCaseSensitive ? std::regex::flag_type() : std::regex_constants::icase);
			isRegEx = true;
		}
		catch (...)
		{
			re = std::regex();
			result = false;
		}
	}
	else
	{
		// Split into substrings
		SimpleStringTokenizer<char> st(s, ' ');
		string tok;
		while (st.getNextNonEmptyToken(tok))
			stringSearches.push_back(StringSearch(tok, !search.isCaseSensitive));
	}
	return result;
}

bool ADLSearchManager::SearchContextItem::matchFile(const string& fullPath, const DirectoryListing::File* file) const noexcept
{
	if (sourceType == ADLSearch::OnlyFile || sourceType == ADLSearch::FullPath || sourceType == ADLSearch::TTH)
	{
		if (minFileSize >= 0 && file->getSize() < (minFileSize << (10*typeFileSize)))
			return false;
		if (maxFileSize >= 0 && file->getSize() > (maxFileSize << (10*typeFileSize)))
			return false;
	}

	switch (sourceType)
	{
		case ADLSearch::OnlyFile:
			return matchString(file->getName());
		case ADLSearch::FullPath:
			return matchString(fullPath);
		case ADLSearch::TTH:
			return file->getTTH() == tth;
	}
	return false;
}

bool ADLSearchManager::SearchContextItem::matchDirectory(const DirectoryListing::Directory* dir) const noexcept
{
	return sourceType == ADLSearch::OnlyDirectory && matchString(dir->getName());
}

bool ADLSearchManager::SearchContextItem::matchString(const string& s) const noexcept
{
	if (isRegEx)
		return std::regex_search(s, re);

	if (!stringSearches.empty())
	{
		if (stringSearches[0].getIgnoreCase())
		{
			string ls = Text::toLower(s);
			for (const auto& ss : stringSearches)
				if (!ss.matchKeepCase(ls))
					return false;
		}
		else
		{
			for (const auto& ss : stringSearches)
				if (!ss.matchKeepCase(s))
					return false;
		}
		return true;
	}
	return false;
}

ADLSearchManager::ADLSearchManager() : csCollection(RWLock::create()), modified(false)
{
	load();
}

ADLSearchManager::~ADLSearchManager()
{
	if (modified) saveL();
}

void ADLSearchManager::load() noexcept
{
	WRITE_LOCK(*csCollection);
	modified = false;
	nextSaveTime = std::numeric_limits<uint64_t>::max();
	collection.clear();
	try
	{
		SimpleXML xml;
		xml.fromXML(File(getConfigFile(), File::READ, File::OPEN).read());
		if (xml.findChild(strADLSearch))
		{
			xml.stepIn();
			if (xml.findChild(strSearchGroup)) // Currenly there is only one search group
			{
				xml.stepIn();
				while (xml.findChild(strSearch))
				{
					xml.stepIn();
					ADLSearch search;
					while (xml.getNextChild())
					{
						const string& tag = xml.getChildTag();
						if (tag == strSearchString)
						{
							search.searchString = xml.getChildData();
							if (xml.getBoolChildAttrib(strRegEx)) search.isRegEx = true;
						}
						else if (tag == strSourceType)
							search.sourceType = search.stringToSourceType(xml.getChildData());
						else if (tag == strDestDirectory)
							search.destDir = xml.getChildData();
						else if (tag == strIsActive)
							search.isActive = Util::toInt(xml.getChildData()) != 0;
						else if (tag == strIsForbidden)
							search.isForbidden = Util::toInt(xml.getChildData()) != 0;
						else if (tag == strUserCommand)
							search.userCommand = xml.getChildData();
						else if (tag == strMaxSize)
							search.maxFileSize = Util::toInt64(xml.getChildData());
						else if (tag == strMinSize)
							search.minFileSize = Util::toInt64(xml.getChildData());
						else if (tag == strSizeType)
							search.typeFileSize = search.stringToSizeType(xml.getChildData());
						else if (tag == strIsAutoQueue)
							search.isAutoQueue = Util::toInt(xml.getChildData()) != 0;
						else if (tag == strIsRegExp)
							search.isRegEx = Util::toInt(xml.getChildData()) != 0;
						else if (tag == strIsCaseSensitive)
							search.isCaseSensitive = Util::toInt(xml.getChildData()) != 0;
					}
					if (!search.searchString.empty())
						collection.push_back(search);
					xml.stepOut();
				}
			}
		}
	}
	catch (const SimpleXMLException&) {}
	catch (const FileException&) {}
}

void ADLSearchManager::saveL() const noexcept
{
	try
	{
		SimpleXML xml;
		xml.addTag(strADLSearch);
		xml.stepIn();
		xml.addTag(strSearchGroup);
		xml.stepIn();

		// Save all searches
		for (const ADLSearch& search : collection)
		{
			if (search.searchString.empty()) continue;
			xml.addTag(strSearch);
			xml.stepIn();

			xml.addTag(strSearchString, search.searchString);
			if (search.isRegEx) xml.addChildAttrib(strRegEx, true);
			xml.addTag(strSourceType, search.sourceTypeToString(search.sourceType));
			xml.addTag(strDestDirectory, search.destDir);
			xml.addTag(strIsActive, search.isActive);
			xml.addTag(strIsForbidden, search.isForbidden);
			if (!search.userCommand.empty()) xml.addTag(strUserCommand, search.userCommand);
			xml.addTag(strMaxSize, search.maxFileSize);
			xml.addTag(strMinSize, search.minFileSize);
			xml.addTag(strSizeType, search.sizeTypeToString(search.typeFileSize));
			xml.addTag(strIsAutoQueue, search.isAutoQueue);
			if (search.isRegEx) xml.addTag(strIsRegExp, true);
			if (search.isCaseSensitive) xml.addTag(strIsCaseSensitive, true);
			xml.stepOut();
		}

		xml.stepOut();
		xml.stepOut();

		// Save string to file
		File fout(getConfigFile(), File::WRITE, File::CREATE | File::TRUNCATE);
		fout.write(SimpleXML::utf8Header);
		fout.write(xml.toXML());
		fout.close();
	}
	catch (const FileException&) {}
	catch (const SimpleXMLException&) {}
}

void ADLSearchManager::save() noexcept
{
	{
		WRITE_LOCK(*csCollection);
		modified = false;
		nextSaveTime = std::numeric_limits<uint64_t>::max();
	}
	READ_LOCK(*csCollection);
	saveL();
}

void ADLSearchManager::saveOnTimer(uint64_t tick) noexcept
{
	{
		WRITE_LOCK(*csCollection);
		if (!modified || tick < nextSaveTime) return;
		modified = false;
		nextSaveTime = std::numeric_limits<uint64_t>::max();
	}
	READ_LOCK(*csCollection);
	saveL();
}

void ADLSearchManager::setDirtyL()
{
	modified = true;
	nextSaveTime = GET_TICK() + 60000;
}

bool ADLSearchManager::isEmpty() const
{
	READ_LOCK(*csCollection);
	return collection.empty();
}

void ADLSearchManager::prepare(ADLSearchManager::SearchContext& ctx, DirectoryListing* dl) const noexcept
{
	if (collection.empty()) return;

	StringMap params;
	DirectoryListing::Directory* root = dl->getRoot();
	UserPtr user = dl->getUser();
	if (user)
	{
		params["userNI"] = params["nick"] = dl->getHintedUser().getNick();
		params["userCID"] = user->getCID().toBase32();
	}
	ctx.dl = dl;
	ctx.user = user;
	ctx.sentCommands.clear();
	ctx.wantFullPath = false;

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	ctx.breakOnFirst = ss->getBool(Conf::ADLS_BREAK_ON_FIRST);
	ss->unlockRead();

	ctx.destDir.emplace_back(DestDir{ defDestDir, new DirectoryListing::AdlDirectory(Util::emptyString, root, "<<<" + defDestDir + ">>>") });

	SearchContextItem item;
	READ_LOCK(*csCollection);
	for (const ADLSearch& search : collection)
		if (search.isActive && item.prepare(search, params))
		{
			if (item.sourceType == ADLSearch::FullPath)
				ctx.wantFullPath = true;
			if (search.destDir.empty())
				item.destDirIndex = 0;
			else
			{
				bool isNew = true;
				for (size_t index = 0; index < ctx.destDir.size(); ++index)
				{
					if (stricmp(search.destDir, ctx.destDir[index].name) == 0)
					{
						item.destDirIndex = index;
						isNew = false;
						break;
					}
				}
				if (isNew)
				{
					item.destDirIndex = ctx.destDir.size();
					DirectoryListing::AdlDirectory* dir;
					if (Text::asciiEqual(search.destDir, strDiscard))
						dir = nullptr;
					else
						dir = new DirectoryListing::AdlDirectory(Util::emptyString, root, "<<<" + search.destDir + ">>>");
					ctx.destDir.emplace_back(DestDir{ search.destDir, dir });
				}
			}
			ctx.collection.emplace_back(std::move(item));
		}
}

void ADLSearchManager::SearchContext::match() noexcept
{
	using Directory = DirectoryListing::Directory;

	if (!dl) return;
	vector<int> tmp;
	const Directory* current = dl->getRoot();

	tmp.push_back(-1);
	while (current)
	{
		if (abortFlag && abortFlag->load()) break;
		int& pos = tmp.back();
		if (pos < 0)
		{
			for (size_t i = 0; i < current->files.size(); i++)
				matchFile(current->files[i]);
			pos = 0;
		}
		if (pos < (int) current->directories.size())
		{
			Directory* dir = current->directories[pos];
			matchDirectory(dir);
			current = dir;
			tmp.push_back(-1);
			continue;
		}
		tmp.pop_back();
		if (tmp.empty()) break;
		tmp.back()++;
		const Directory *parent = current->getParent();
		dcassert(parent);
		current = parent;
	}
}

void ADLSearchManager::copyDirectory(DirectoryListing::Directory* adlsDestDir, const DirectoryListing::Directory* src, const DirectoryListing* dl) noexcept
{
	using AdlDirectory = DirectoryListing::AdlDirectory;
	using AdlFile = DirectoryListing::AdlFile;
	using Directory = DirectoryListing::Directory;
	using File = DirectoryListing::File;
	vector<int> tmp;
	const Directory* current = src;
	string path = dl->getPath(src);
	Directory* dest = new AdlDirectory(path, adlsDestDir, src->getName());
	dest->setFlag(src->getFlags() & DirectoryListing::DIR_STATUS_FLAGS);
	adlsDestDir->directories.push_back(dest);

	tmp.push_back(-1);
	while (current)
	{
		int& pos = tmp.back();
		if (pos < 0)
		{
			for (size_t i = 0; i < current->files.size(); i++)
			{
				const File* srcFile = current->files[i];
				File* file = new AdlFile(dl->getPath(srcFile->getParent()), *srcFile);
				file->setFlag(srcFile->getFlags() & DirectoryListing::FILE_STATUS_FLAGS);
				dest->addFile(file);
			}
			pos = 0;
		}
		if (pos < (int) current->directories.size())
		{
			src = current->directories[pos];
			AdlDirectory* dir = new AdlDirectory(dl->getPath(src), dest, src->getName());
			dest->directories.push_back(dir);
			dir->setFlag(src->getFlags() & DirectoryListing::DIR_STATUS_FLAGS);
			dest = dir;
			current = src;
			tmp.push_back(-1);
			continue;
		}
		tmp.pop_back();
		if (tmp.empty()) break;
		tmp.back()++;
		const Directory *parent = current->getParent();
		dcassert(parent);
		current = parent;
		dest = dest->getParent();
	}
}

void ADLSearchManager::matchListing(DirectoryListing* dl, std::atomic_bool* abortFlag) const noexcept
{
	SearchContext ctx;
	ctx.abortFlag = abortFlag;
	prepare(ctx, dl);
	ctx.match();
	if (!(abortFlag && abortFlag->load()))
		ctx.insertResults();
}

ADLSearchManager::SearchContext::~SearchContext()
{
	for (auto& d : destDir)
		delete d.dir;
}

bool ADLSearchManager::SearchContext::matchFile(const DirectoryListing::File* file) noexcept
{
	bool getConnFlag = true;
	bool result = false;
	string fullPath;
	UserCommand uc;
	for (const auto& item : collection)
	{
		if (item.sourceType == ADLSearch::OnlyDirectory) continue;
		if (wantFullPath) fullPath = dl->getPath(file) + file->getName();
		if (!item.matchFile(fullPath, file)) continue;
		DirectoryListing::AdlDirectory* dir = destDir[item.destDirIndex].dir;
		if (!dir) continue;
		DirectoryListing::AdlFile* newFile = new DirectoryListing::AdlFile(dl->getPath(file->getParent()), *file);
		newFile->setFlag(file->getFlags() & DirectoryListing::FILE_STATUS_FLAGS);
		dir->addFile(newFile);
		if (item.isAutoQueue)
		{
			try
			{
				QueueManager::QueueItemParams params;
				params.size = file->getSize();
				params.root = &file->getTTH();
				QueueManager::getInstance()->add(file->getName(), params, dl->getHintedUser(), 0, 0, getConnFlag);
			}
			catch (const Exception& e)
			{
				LogManager::message(e.getError());
			}
		}
		if (item.userCommandId != -1 && FavoriteManager::getInstance()->getUserCommand(item.userCommandId, uc))
		{
			if (!uc.once() || sentCommands.find(item.userCommandId) == sentCommands.end())
			{
				StringMap ucParams;
				dl->getFileParams(file, ucParams);
				ClientManager::userCommand(dl->getHintedUser(), uc, ucParams, true);
				if (uc.once()) sentCommands.insert(item.userCommandId);
			}
		}
		result = true;
		if (breakOnFirst) break;
	}
	return result;
}

bool ADLSearchManager::SearchContext::matchDirectory(const DirectoryListing::Directory* dir) noexcept
{
	bool result = false;
	for (const auto& item : collection)
	{
		if (!item.matchDirectory(dir)) continue;
		if (destDir[item.destDirIndex].dir)
			ADLSearchManager::copyDirectory(destDir[item.destDirIndex].dir, dir, dl);
#if 0
		if (item.isAutoQueue)
		{
			// TODO
		}
#endif
		result = true;
		if (breakOnFirst) break;
	}
	return result;
}

void ADLSearchManager::SearchContext::insertResults() noexcept
{
	if (!dl) return;
	DirectoryListing::Directory* root = dl->getRoot();
	for (auto& dd : destDir)
	{
		if (!dd.dir) continue;
		if (!dd.dir->files.empty() || !dd.dir->directories.empty())
		{
			dd.dir->updateFlags();
			root->directories.push_back(dd.dir);
			dd.dir = nullptr;
		}
		else
		{
			delete dd.dir;
			dd.dir = nullptr;
		}
	}
}

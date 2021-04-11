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
#include "QueueManager.h"
#include "LogManager.h"
#include "SimpleStringTokenizer.h"
#include "ParamExpander.h"

#ifdef IRAINMAN_INCLUDE_USER_CHECK
#include "ClientManager.h"
#endif

ADLSearch::ADLSearch() :
	searchString("<Enter string>"),
	isActive(true),
	isAutoQueue(false),
	sourceType(OnlyFile),
	minFileSize(-1),
	maxFileSize(-1),
	typeFileSize(SizeBytes),
	destDir("ADLSearch"),
	ddIndex(0),
	isForbidden(false),
	raw(0)
{
}

static const string strFilename("Filename");
static const string strDirectory("Directory");
static const string strFullPath("Full Path");

static const string strKB("kB");
static const string strMB("MB");
static const string strGB("GB");
static const string strB("B");

ADLSearch::SourceType ADLSearch::stringToSourceType(const string& s)
{
	if (stricmp(s, strFilename) == 0)
		return OnlyFile;
	if (stricmp(s, strDirectory) == 0)
		return OnlyDirectory;
	if (stricmp(s, strFullPath) == 0)
		return FullPath;
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
	}
}

const tstring& ADLSearch::sourceTypeToDisplayString(SourceType t)
{
	switch (t)
	{
		default:
		case OnlyFile:
			return TSTRING(FILENAME);
		case OnlyDirectory:
			return TSTRING(DIRECTORY);
		case FullPath:
			return TSTRING(ADLS_FULL_PATH);
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

const tstring& ADLSearch::sizeTypeToDisplayString(ADLSearch::SizeType t)
{
	switch (t)
	{
		default:
		case SizeBytes:
			return TSTRING(B);
		case SizeKiloBytes:
			return TSTRING(KB);
		case SizeMegaBytes:
			return TSTRING(MB);
		case SizeGigaBytes:
			return TSTRING(GB);
	}
}

void ADLSearch::prepare(StringMap& params)
{
	// Prepare quick search of substrings
	stringSearches.clear();
	
	// Replace parameters such as %[nick]
	const string s = Util::formatParams(searchString, params, false);
	
	// Split into substrings
	SimpleStringTokenizer<char> st(s, ' ');
	string tok;
	while (st.getNextNonEmptyToken(tok))
		stringSearches.push_back(StringSearch(tok));
}

inline void ADLSearch::unprepare()
{
	stringSearches.clear();
}

bool ADLSearch::matchesFile(const string& f, const string& fp, int64_t size) const
{
	// Check status
	if (!isActive)
		return false;
	
	// Check size for files
	if (size >= 0 && (sourceType == OnlyFile || sourceType == FullPath))
	{
		if (minFileSize >= 0 && size < (minFileSize << (10*typeFileSize)))
			return false;
		if (maxFileSize >= 0 && size > (maxFileSize << (10*typeFileSize)))
			return false;
	}
	
	// Do search
	switch (sourceType)
	{
		default:
		case OnlyDirectory:
			return false;
		case OnlyFile:
			return searchAll(f);
		case FullPath:
			return searchAll(fp);
	}
}

bool ADLSearch::matchesDirectory(const string& d) const
{
	// Check status
	if (!isActive)
	{
		return false;
	}
	if (sourceType != OnlyDirectory)
	{
		return false;
	}
	
	// Do search
	return searchAll(d);
}

bool ADLSearch::searchAll(const string& s) const
{
	try
	{
		std::regex reg(searchString, std::regex_constants::icase);
		return std::regex_search(s, reg);
	}
	catch (...) {}
	
	// Match all substrings
	for (auto i = stringSearches.cbegin(), iend = stringSearches.cend(); i != iend; ++i)
	{
		if (!i->match(s))
		{
			return false;
		}
	}
	return !stringSearches.empty();
}

ADLSearchManager::ADLSearchManager() : breakOnFirst(false), sentRaw(false)
{
	load();
}

ADLSearchManager::~ADLSearchManager()
{
	save();
}

void ADLSearchManager::load()
{
	// Clear current
	collection.clear();
	
	// Load file as a string
	try
	{
		SimpleXML xml;
		xml.fromXML(File(getConfigFile(), File::READ, File::OPEN).read());
		
		if (xml.findChild("ADLSearch"))
		{
			xml.stepIn();
			
			// Predicted several groups of searches to be differentiated
			// in multiple categories. Not implemented yet.
			if (xml.findChild("SearchGroup"))
			{
				xml.stepIn();
				
				// Loop until no more searches found
				while (xml.findChild("Search"))
				{
					xml.stepIn();
					
					// Found another search, load it
					ADLSearch search;
					
					if (xml.findChild("SearchString"))
					{
						search.searchString = xml.getChildData();
					}
					if (xml.findChild("SourceType"))
					{
						search.sourceType = search.stringToSourceType(xml.getChildData());
					}
					if (xml.findChild("DestDirectory"))
					{
						search.destDir = xml.getChildData();
					}
					if (xml.findChild("IsActive"))
					{
						search.isActive = (Util::toInt(xml.getChildData()) != 0);
					}
					if (xml.findChild("IsForbidden"))
					{
						search.isForbidden = (Util::toInt(xml.getChildData()) != 0);
					}
					if (xml.findChild("Raw"))
					{
						search.raw = Util::toInt(xml.getChildData());
					}
					if (xml.findChild("MaxSize"))
					{
						search.maxFileSize = Util::toInt64(xml.getChildData());
					}
					if (xml.findChild("MinSize"))
					{
						search.minFileSize = Util::toInt64(xml.getChildData());
					}
					if (xml.findChild("SizeType"))
					{
						search.typeFileSize = search.stringToSizeType(xml.getChildData());
					}
					if (xml.findChild("IsAutoQueue"))
					{
						search.isAutoQueue = (Util::toInt(xml.getChildData()) != 0);
					}
					
					// Add search to collection
					if (!search.searchString.empty())
					{
						collection.push_back(search);
					}
					
					// Go to next search
					xml.stepOut();
				}
			}
		}
	}
	catch (const SimpleXMLException&) {}
	catch (const FileException&) {}
}

void ADLSearchManager::save() const
{
	// Prepare xml string for saving
	try
	{
		SimpleXML xml;
		
		xml.addTag("ADLSearch");
		xml.stepIn();
		
		// Predicted several groups of searches to be differentiated
		// in multiple categories. Not implemented yet.
		xml.addTag("SearchGroup");
		xml.stepIn();
		
		// Save all searches
		for (auto i = collection.cbegin(); i != collection.cend(); ++i)
		{
			const ADLSearch& search = *i;
			if (search.searchString.empty())
				continue;
			xml.addTag("Search");
			xml.stepIn();

			xml.addTag("SearchString", search.searchString);
			xml.addTag("SourceType", search.sourceTypeToString(search.sourceType));
			xml.addTag("DestDirectory", search.destDir);
			xml.addTag("IsActive", search.isActive);
			xml.addTag("IsForbidden", search.isForbidden);
			xml.addTag("Raw", search.raw);
			xml.addTag("MaxSize", search.maxFileSize);
			xml.addTag("MinSize", search.minFileSize);
			xml.addTag("SizeType", search.sizeTypeToString(search.typeFileSize));
			xml.addTag("IsAutoQueue", search.isAutoQueue);
			xml.stepOut();
		}
		
		xml.stepOut();
		
		xml.stepOut();
		
		// Save string to file
		try
		{
			File fout(getConfigFile(), File::WRITE, File::CREATE | File::TRUNCATE);
			fout.write(SimpleXML::utf8Header);
			fout.write(xml.toXML());
			fout.close();
		}
		catch (const FileException&) {}
	}
	catch (const SimpleXMLException&) {}
}

void ADLSearchManager::matchesFile(DestDirList& destDirVector, const DirectoryListing::File *currentFile, const string& fullPath)
{
	// Add to any substructure being stored
	for (auto id = destDirVector.begin(); id != destDirVector.end(); ++id)
	{
		if (id->subdir != NULL)
		{
			auto copyFile = new DirectoryListing::File(*currentFile);
			copyFile->setAdls(true);
			copyFile->setFlags(currentFile->getFlags());
			dcassert(id->subdir->getAdls());
			
			id->subdir->files.push_back(copyFile);
		}
		id->fileAdded = false;  // Prepare for next stage
	}
	
	// Prepare to match searches
	if (currentFile->getName().empty())
		return;
	
	string filePath = fullPath + "\\" + currentFile->getName();
	// Match searches
	for (auto is = collection.cbegin(); is != collection.cend(); ++is)
	{
		if (destDirVector[is->ddIndex].fileAdded)
		{
			continue;
		}
		if (is->matchesFile(currentFile->getName(), filePath, currentFile->getSize()))
		{
			auto copyFile = new DirectoryListing::File(*currentFile);
			copyFile->setAdls(true);
			copyFile->setFlags(currentFile->getFlags());
#ifdef IRAINMAN_INCLUDE_USER_CHECK
			if (is->isForbidden && !getSentRaw())
			{
				unique_ptr<char[]> buf(new char[FULL_MAX_PATH]);
				_snprintf(buf.get(), FULL_MAX_PATH, CSTRING(CHECK_FORBIDDEN), currentFile->getName().c_str());
				
				ClientManager::setClientStatus(user, buf.get(), is->raw, false);
				
				setSentRaw(true);
			}
#endif

			destDirVector[is->ddIndex].dir->files.push_back(copyFile);
			destDirVector[is->ddIndex].fileAdded = true;
			
			if (is->isAutoQueue)
			{
				try
				{
					bool getConnFlag = true;
					QueueManager::getInstance()->add(currentFile->getName(),
						currentFile->getSize(), currentFile->getTTH(), getUser(), 0, QueueItem::DEFAULT, true, getConnFlag);
				}
				catch (const Exception& e)
				{
					LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
				}
			}
			
			if (breakOnFirst)
			{
				// Found a match, search no more
				break;
			}
		}
	}
}

void ADLSearchManager::matchesDirectory(DestDirList& destDirVector, const DirectoryListing::Directory* currentDir, const string& fullPath) const
{
	// Add to any substructure being stored
	for (auto id = destDirVector.begin(); id != destDirVector.end(); ++id)
	{
		if (id->subdir)
		{
			DirectoryListing::Directory* newDir =
			    new DirectoryListing::AdlDirectory(fullPath, id->subdir, currentDir->getName());
			id->subdir->directories.push_back(newDir);
			id->subdir = newDir;
		}
	}
	
	// Prepare to match searches
	if (currentDir->getName().size() < 1)
	{
		return;
	}
	
	// Match searches
	for (auto is = collection.cbegin(); is != collection.cend(); ++is)
	{
		if (destDirVector[is->ddIndex].subdir)
			continue;
		if (is->matchesDirectory(currentDir->getName()))
		{
			destDirVector[is->ddIndex].subdir =
			    new DirectoryListing::AdlDirectory(fullPath, destDirVector[is->ddIndex].dir, currentDir->getName());
			destDirVector[is->ddIndex].dir->directories.push_back(destDirVector[is->ddIndex].subdir);
			if (breakOnFirst)
			{
				// Found a match, search no more
				break;
			}
		}
	}
}

void ADLSearchManager::stepUpDirectory(DestDirList& destDirVector) const
{
	for (auto id = destDirVector.begin(); id != destDirVector.end(); ++id)
	{
		if (id->subdir != nullptr)
		{
			id->subdir = id->subdir->getParent();
			if (id->subdir == id->dir)
			{
				id->subdir = nullptr;
			}
		}
	}
}

void ADLSearchManager::prepareDestinationDirectories(DestDirList& destDirVector, DirectoryListing::Directory* root, StringMap& params)
{
	// Load default destination directory (index = 0)
	destDirVector.clear();
	auto id = destDirVector.insert(destDirVector.end(), DestDir());
	id->name = "ADLSearch";
	id->dir  = new DirectoryListing::Directory(root, "<<<" + id->name + ">>>", true, true);
	
	// Scan all loaded searches
	for (auto is = collection.begin(); is != collection.end(); ++is)
	{
		// Check empty destination directory
		if (is->destDir.empty())
		{
			// Set to default
			is->ddIndex = 0;
			continue;
		}
		
		// Check if exists
		bool isNew = true;
		size_t ddIndex = 0;
		for (id = destDirVector.begin(); id != destDirVector.end(); ++id, ++ddIndex)
		{
			if (stricmp(is->destDir.c_str(), id->name.c_str()) == 0)
			{
				// Already exists, reuse index
				is->ddIndex = ddIndex;
				isNew = false;
				break;
			}
		}
		
		if (isNew)
		{
			// Add new destination directory
			id = destDirVector.insert(destDirVector.end(), DestDir());
			id->name = is->destDir;
			id->dir = new DirectoryListing::Directory(root, "<<<" + id->name + ">>>", true, true);
			is->ddIndex = ddIndex;
		}
	}
	// Prepare all searches
	for (auto ip = collection.begin(); ip != collection.end(); ++ip)
	{
		ip->prepare(params);
	}
}

void ADLSearchManager::finalizeDestinationDirectories(DestDirList& destDirVector, DirectoryListing::Directory* root)
{
	string szDiscard("<<<" + STRING(ADLS_DISCARD) + ">>>");
	
	// Add non-empty destination directories to the top level
	for (auto id = destDirVector.begin(); id != destDirVector.end(); ++id)
	{
		if (id->dir->files.empty() && id->dir->directories.empty())
		{
			delete id->dir;
		}
		else if (stricmp(id->dir->getName(), szDiscard) == 0)
		{
			delete id->dir;
		}
		else
		{
			root->directories.push_back(id->dir);
		}
	}
	
	for (auto ip = collection.begin(); ip != collection.end(); ++ip)
	{
		ip->unprepare();
	}
}

void ADLSearchManager::matchListing(DirectoryListing& dl) noexcept
{
	if (collection.empty())
		return;
	StringMap params;
	if (dl.getUser())
	{
		params["userNI"] = dl.getUser()->getLastNick();
		params["userCID"] = dl.getUser()->getCID().toBase32();
	}
	setUser(dl.getUser());
	setSentRaw(false);
	
	DestDirList destDirs;
	prepareDestinationDirectories(destDirs, dl.getRoot(), params);
	setBreakOnFirst(BOOLSETTING(ADLS_BREAK_ON_FIRST));
	
	const string path(dl.getRoot()->getName());
	matchRecurse(destDirs, dl.getRoot(), path);
	
	finalizeDestinationDirectories(destDirs, dl.getRoot());
}

void ADLSearchManager::matchRecurse(DestDirList& destList, const DirectoryListing::Directory* dir, const string& path)
{
	for (const DirectoryListing::Directory* d : dir->directories)
	{
		string tmpPath = path + "\\" + d->getName();
		matchesDirectory(destList, d, tmpPath);
		matchRecurse(destList, d, tmpPath);
	}
	for (const DirectoryListing::File* f : dir->files)
	{
		matchesFile(destList, f, path);
	}
	stepUpDirectory(destList);
}

string ADLSearchManager::getConfigFile()
{
	return Util::getConfigPath() + "ADLSearch.xml";
}

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

#include "stdinc.h"
#include "ShareManager.h"
#include "Transfer.h"
#include "FileTypes.h"
#include "AdcCommand.h"
#include "AdcHub.h"
#include "SimpleXML.h"
#include "SimpleXMLReader.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#include "ClientManager.h"
#include "HashBloom.h"
#include "HashManager.h"
#include "Wildcards.h"
#include "StringTokenizer.h"
#include "DatabaseManager.h"
#include "LogManager.h"
#include "DebugManager.h"
#include "TimeUtil.h"
#include "Tag16.h"
#include "unaligned.h"

STANDARD_EXCEPTION(ShareLoaderException);
STANDARD_EXCEPTION(ShareWriterException);

#undef DEBUG_FILELIST
#undef DEBUG_SHARE_MANAGER

static const string tagFileListing = "FileListing";
static const string tagDirectory = "Directory";
static const string tagFile = "File";
static const string attrName = "Name";
static const string attrSize = "Size";
static const string attrTTH = "TTH";
static const string attrHit = "HIT";
static const string attrTS = "TS";
static const string attrShared = "Shared";

static const string fileShareData("Share.dat");
static const string fileBZXml("files.xml.bz2");
static const string fileAttrXml("FileAttr.xml");

enum
{
	SCAN_SHARE_FLAG_ADDED         = 1,
	SCAN_SHARE_FLAG_REMOVED       = 2,
	SCAN_SHARE_FLAG_REBUILD_BLOOM = 4
};

enum
{
	MODE_FULL_LIST,
	MODE_PARTIAL_LIST,
	MODE_RECURSIVE_PARTIAL_LIST
};

static const size_t MAX_PARTIAL_LIST_SIZE = 512 * 1024;

class ShareLoader : public SimpleXMLReader::CallBack
{
	public:
		ShareLoader(ShareManager& manager): manager(manager), current(nullptr), inListing(false)
		{
		}
		
		void startTag(const string& name, StringPairList& attribs, bool simple);
		void endTag(const string& name, const string& data);
		
	private:
		ShareManager& manager;
		SharedDir* current;
		bool inListing;
};

void ShareLoader::startTag(const string& name, StringPairList& attribs, bool simple)
{
#if 0
	if (manager.stopLoading)
		throw ShareLoaderException("Stopped");
#endif
	if (inListing)
	{
		if (name == tagFile)
		{
			if (!current) return;

			const string *valFilename = nullptr;
			const string *valTTH = nullptr;
			const string *valSize = nullptr;
			const string *valTS = nullptr;
			const string *valShared = nullptr;

			for (auto i = attribs.cbegin(); i != attribs.cend(); i++)
			{
				const string &value = i->second;
				if (value.empty()) continue; // all values should be non-empty
				const string &attrib = i->first;
				if (attrib == attrName) valFilename = &value; else
				if (attrib == attrTTH) valTTH = &value; else
				if (attrib == attrSize) valSize = &value; else
				if (attrib == attrTS) valTS = &value; else
				if (attrib == attrShared) valShared = &value;
			}
			
			if (!valFilename) return;
			if (!valSize) return;
			if (!valTTH || valTTH->length() != 39) return;
			
			TTHValue tth;
			bool error;
			Encoder::fromBase32(valTTH->c_str(), tth.data, sizeof(tth.data), &error);
			if (error) return;

			int64_t size = Util::toInt64(*valSize);
			if (size < 0) return;

			uint64_t timeShared = 0;
			if (valShared)
			{
				int64_t val = Util::toInt64(*valShared);
				if (val > 0) timeShared = val;
			} else
			if (valTS)
			{
				int64_t val = Util::toInt64(*valTS);
				if (val > 0) timeShared = val * (int64_t) Util::FILETIME_UNITS_PER_SEC + 116444736000000000ll;
			}

			manager.loadSharedFile(current, *valFilename, size, tth, 0, timeShared);
		}
		else if (name == tagDirectory)
		{
			const string& fileName = getAttrib(attribs, attrName, 0);
			if (fileName.empty()) throw ShareLoaderException("Empty directory name");
			manager.loadSharedDir(current, fileName);
			if (simple) endTag(name, Util::emptyString);
		}
	}
	else if (name == tagFileListing)
	{
		inListing = true;
		if (simple) endTag(name, Util::emptyString);
	}
}

void ShareLoader::endTag(const string& name, const string&)
{
	if (!inListing) return;
	if (name == tagDirectory)
	{
		if (current)
		{
			if (current->parent)
			{
				current->parent->dirsTypesMask |= current->getTypes();
				current->parent->totalSize += current->totalSize;
			}
			current = current->parent;
		}
	}
	else if (name == tagFileListing)
	{
		inListing = false;
	}
}

string ShareManager::validateVirtual(const string& virt) noexcept
{
	string tmp = virt;
	for (size_t i = 0; i < tmp.length(); i++)
		if (tmp[i] == '/' || tmp[i] == '\\')
			tmp[i] = '_';
	return tmp;
}

void ShareManager::loadSharedFile(SharedDir* current, const string& filename, int64_t size, const TTHValue& tth, uint64_t timestamp, uint64_t timeShared) noexcept
{
	SharedFilePtr file = std::make_shared<SharedFile>(filename, tth, size, timestamp, timeShared, getFileTypesFromFileName(filename));
	current->files.insert(make_pair(file->getLowerName(), file));
	current->filesTypesMask |= file->getFileTypes();
	current->totalSize += file->getSize();	
	fileCounter++;
	bloom.add(file->getLowerName());

	ShareManager::TTHMapItem tthItem;
	tthItem.dir = current;
	tthItem.file = file;
	tthIndex.insert(make_pair(tth, tthItem));
}

void ShareManager::loadSharedDir(SharedDir* &current, const string& filename) noexcept
{
	if (current)
	{
		SharedDir* dir = new SharedDir(filename, current);
		if (current->flags & BaseDirItem::FLAG_SHARE_LOST)
			dir->flags |= BaseDirItem::FLAG_SHARE_LOST;
		else
			bloom.add(dir->getLowerName());
		current->dirs.insert(make_pair(dir->getLowerName(), dir));
		current = dir;
	}
	else
	{
		auto it = getByVirtualL(filename);
		if (it != shares.cend())
		{
			current = it->dir;
			fileCounter = 0;
		}
	}			
}

void ShareManager::loadShareList(SimpleXML& xml)
{
	WRITE_LOCK(*csShare);
	
	xml.resetCurrentChild();
	if (xml.findChild("Share"))
	{
		xml.stepIn();
		while (xml.findChild("Directory"))
		{
			string realPath = xml.getChildData();
			if (realPath.empty())
			{
				continue;
			}
			// make sure realPath ends with a PATH_SEPARATOR
			Util::appendPathSeparator(realPath);
			
			const string& virtualAttr = xml.getChildAttrib("Virtual");
			const string virtualName = validateVirtual(virtualAttr.empty() ? Util::getLastDir(realPath) : virtualAttr);
			bool unused;
			if (hasShareL(virtualName, realPath, unused))
			{
				LogManager::message("Duplicate share: real=" + realPath + " virtual=" + virtualAttr + ", skipping...", false);
				continue;
			}
			if (!File::isExist(realPath)) // TODO: is a directory?
				continue;
			ShareListItem sli;
			sli.realPath.setName(realPath);
			sli.dir = new SharedDir(virtualName, nullptr);
			sli.version = 0;
			sli.totalFiles = 0;
			sli.flags = 0;
			bloom.add(sli.dir->getLowerName());
			shares.push_back(sli);
		}
		xml.stepOut();
	}

	xml.resetCurrentChild();
	if (xml.findChild("NoShare"))
	{
		xml.stepIn();
		while (xml.findChild("Directory"))
		{
			string excludePath = xml.getChildData();
			Util::appendPathSeparator(excludePath);
			addExcludeFolderL(excludePath);
		}			
		xml.stepOut();
	}

	xml.resetCurrentChild();
	if (xml.findChild("ShareGroups"))
	{
		xml.stepIn();
		while (xml.findChild("ShareGroup"))
		{
			string name = xml.getChildAttrib("Name");
			const string& id = xml.getChildAttrib("ID");
			CID cid;
			if (!id.empty()) cid.fromBase32(id);
			xml.stepIn();
			list<string> shares;
			while (xml.findChild("Directory"))
			{
				string dir = xml.getChildData();
				Util::appendPathSeparator(dir);
				auto i = getByRealL(dir);
				if (i == this->shares.cend())
				{
					LogManager::message("Share " + dir + " not found, skipping...", false);
					continue;
				}
				shares.push_back(dir);
			}
			try
			{
				const FileAttr* useAttr = nullptr;
				FileAttr attr[MAX_FILE_ATTR];
				if (!cid.isZero())
				{
					string path = Util::getConfigPath() + "ShareGroups" PATH_SEPARATOR_STR + cid.toBase32() + PATH_SEPARATOR + fileAttrXml;
					CID fileCID;
					if (readFileAttr(path, attr, fileCID))
						useAttr = attr;
					if (autoRefreshMode == REFRESH_MODE_NONE && fileCID != ClientManager::getMyCID())
						autoRefreshMode = REFRESH_MODE_FILE_LIST;
				}
				addShareGroupL(name, cid, shares, useAttr);
			}
			catch (const ShareException&) {}
			xml.stepOut();
		}
		xml.stepOut();
	}
}

bool ShareManager::ShareGroup::hasShare(const ShareManager::ShareListItem& share) const
{
	const string& name = share.realPath.getLowerName();
	for (const BaseDirItem& item : shares)
		if (name == item.getLowerName())
			return true;
	return false;
}

ShareManager::ShareManager() :
	csShare(RWLock::create()),
	shareListVersion(1),
	totalSize(0),
	totalFiles(0),
	fileCounter(0),
	shareListChanged(false),
	fileListChanged(false),
	versionCounter(0),
	bloom(1<<20),
	hits(0),
	doingScanDirs(false),
	doingHashFiles(false),
	doingCreateFileList(false),
	stopScanning(false),
	finishedScanDirs(false),
	bloomNew(1<<20),
	scanShareFlags(0), scanAllFlags(0),
	nextFileID(0), maxSharedFileID(0), maxHashedFileID(0),
	optionShareHidden(false), optionShareSystem(false), optionShareVirtual(false),
	optionIncludeUploadCount(false), optionIncludeTimestamp(false),
	hashDb(nullptr),
	tickUpdateList(std::numeric_limits<uint64_t>::max()),
	tickLastRefresh(0),
	timeLastRefresh(0),
	tickRestoreFileList(std::numeric_limits<uint64_t>::max()),
	tempFileCount(0),
	hasSkipList(false)
{
	autoRefreshMode = REFRESH_MODE_NONE;
	scanProgress[0] = scanProgress[1] = 0;
	const string fileAttrPath = Util::getConfigPath() + fileAttrXml;
	CID fileCID;
	if (!readFileAttr(fileAttrPath, fileAttr, fileCID) || fileCID != ClientManager::getMyCID())
		autoRefreshMode = REFRESH_MODE_FILE_LIST;
	const string emptyXmlName = getEmptyBZXmlFile();
	if (fileAttr[FILE_ATTR_EMPTY_FILES_XML].size < 0 ||
	    fileAttr[FILE_ATTR_EMPTY_FILES_BZ_XML].size < 0 ||
		!File::isExist(emptyXmlName))
	{
		writeEmptyFileList(emptyXmlName);
		writeFileAttr(fileAttrPath, fileAttr);
	}

	autoRefreshTime = SETTING(AUTO_REFRESH_TIME) * 60000;
	if (BOOLSETTING(AUTO_REFRESH_ON_STARTUP))
	{
		autoRefreshMode = REFRESH_MODE_FULL;
		tickRefresh = 0;
	}
	else if (autoRefreshTime)
		tickRefresh = GET_TICK() + autoRefreshTime;
	else
		tickRefresh = std::numeric_limits<uint64_t>::max();

	HashManager::getInstance()->addListener(this);
	TimerManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
}

ShareManager::~ShareManager()
{
	renameXmlFiles();
	removeOldShareGroupFiles();
	if (!tempShareDataFile.empty())
		File::renameFile(Util::getConfigPath() + tempShareDataFile, Util::getConfigPath() + fileShareData);
	if (hashDb)
		DatabaseManager::getInstance()->putHashDatabaseConnection(hashDb);
}

static const uint8_t SHARE_DATA_DIR_START = 1;
static const uint8_t SHARE_DATA_DIR_END   = 2;
static const uint8_t SHARE_DATA_FILE      = 3;

static const uint8_t SHARE_DATA_ATTRIB_NAME        = 1;
static const uint8_t SHARE_DATA_ATTRIB_SIZE        = 2;
static const uint8_t SHARE_DATA_ATTRIB_TTH         = 3;
static const uint8_t SHARE_DATA_ATTRIB_TIMESTAMP   = 4;
static const uint8_t SHARE_DATA_ATTRIB_TIME_SHARED = 5;
static const uint8_t SHARE_DATA_ATTRIB_HIT         = 6;

void ShareManager::loadShareData(File& file)
{
	enum ShareLoaderMode
	{
		MODE_NONE,
		MODE_FILE,
		MODE_DIR
	};

	enum
	{
		ATTRIB_MASK_NAME        = 0x01,
		ATTRIB_MASK_SIZE        = 0x02,
		ATTRIB_MASK_TTH         = 0x04,
		ATTRIB_MASK_TIMESTAMP   = 0x08,
		ATTRIB_MASK_TIME_SHARED = 0x10
	};

	BufferedInputStream<false> bs(&file, 256 * 1024);
	uint8_t buf[64 * 1024];
	ShareLoaderMode mode = MODE_NONE;
	SharedDir* current = nullptr;
	string name;
	int64_t fileSize = 0;
	uint64_t timestamp = 0;
	uint64_t timeShared = 0;
	TTHValue tth;
	unsigned attribMask = 0;
	for (;;)
	{
#if 0
		if (stopLoading)
			throw ShareLoaderException("Stopped");
#endif
		size_t size = 3;
		bs.read(buf, size);
		if (mode == MODE_NONE)
		{
			if (!size) break;
			if (size > 1) bs.rewind(size-1);
			if (buf[0] == SHARE_DATA_DIR_START)
			{
				attribMask = 0;
				mode = MODE_DIR;
				continue;
			}
			if (buf[0] == SHARE_DATA_FILE)
			{
				if (!current)
					throw ShareLoaderException("Unexpected file tag");
				attribMask = 0;
				mode = MODE_FILE;
				continue;
			}
			if (buf[0] == SHARE_DATA_DIR_END)
			{
				if (!current)
					throw ShareLoaderException("Unexpected directory end tag");
				SharedDir* parent = current->parent;
				if (parent)
				{
					parent->dirsTypesMask |= current->getTypes();
					parent->totalSize += current->totalSize;
				}
				else
				{
					if (!(current->flags & BaseDirItem::FLAG_SHARE_LOST))
					{
						for (ShareListItem& sli : shares)
							if (sli.dir == current)
							{
								sli.totalFiles = fileCounter;
								break;
							}
					}
					else
						SharedDir::deleteTree(current);
				}
				current = parent;
				continue;
			}
			throw ShareLoaderException("Unexpected tag " + Util::toString(buf[0]));
		}

		// read attributes
		if (!size || (buf[0] && size < 2) || ((buf[1] & 0x80) && size < 3))
			throw ShareLoaderException("Unexpected end of tag");
		dcassert(mode == MODE_DIR || mode == MODE_FILE);
		uint8_t type = buf[0];
		if (type == 0)
		{
			if (!(attribMask & ATTRIB_MASK_NAME))
				throw ShareLoaderException(mode == MODE_DIR ? "Directory name is missing" : "File name is missing");
			dcassert(!name.empty());
			if (mode == MODE_DIR)
			{
				loadSharedDir(current, name);
				if (!current)
				{
					current = new SharedDir(name, nullptr);
					current->flags |= BaseDirItem::FLAG_SHARE_LOST;
					LogManager::message("Share " + name + " was removed but kept in " + fileShareData, false);
				}
			}
			else
			{
				const unsigned reqMask = ATTRIB_MASK_SIZE | ATTRIB_MASK_TTH | ATTRIB_MASK_TIMESTAMP;
				if ((attribMask & reqMask) != reqMask)
					throw ShareLoaderException("Required file attributes are missing");
				if (!(current->flags & BaseDirItem::FLAG_SHARE_LOST))
					loadSharedFile(current, name, fileSize, tth, timestamp,
						(attribMask & ATTRIB_MASK_TIME_SHARED) ? timeShared : 0);
			}
			mode = MODE_NONE;
			bs.rewind(size-1);
			continue;
		}
		size_t attribSize;
		if (buf[1] & 0x80)
		{
			attribSize = (buf[1] & 0x7F) << 8 | buf[2];
		} else
		{
			attribSize = buf[1];
			bs.rewind(1);
		}
		size = attribSize;
		if (bs.read(buf, size) != attribSize)
			throw ShareLoaderException("Failed to load attribute " + Util::toString(type));
		bool invalidSize = false;
		switch (type)
		{
			case SHARE_DATA_ATTRIB_NAME:
				if (!attribSize) { invalidSize = true; break; }
				name.assign((const char*) buf, attribSize);
				attribMask |= ATTRIB_MASK_NAME;
				break;
			case SHARE_DATA_ATTRIB_SIZE:
				if (attribSize != 8) { invalidSize = true; break; }
				fileSize = loadUnaligned64(buf);
				attribMask |= ATTRIB_MASK_SIZE;
				break;
			case SHARE_DATA_ATTRIB_TTH:
				if (attribSize != TTHValue::BYTES) { invalidSize = true; break; }
				memcpy(tth.data, buf, TTHValue::BYTES);
				attribMask |= ATTRIB_MASK_TTH;
				break;
			case SHARE_DATA_ATTRIB_TIMESTAMP:
				if (attribSize != 8) { invalidSize = true; break; }
				timestamp = loadUnaligned64(buf);
				attribMask |= ATTRIB_MASK_TIMESTAMP;
				break;
			case SHARE_DATA_ATTRIB_TIME_SHARED:
				if (attribSize != 8) { invalidSize = true; break; }
				timeShared = loadUnaligned64(buf);
				attribMask |= ATTRIB_MASK_TIME_SHARED;
		}
		if (invalidSize)
			throw ShareLoaderException("Invalid size of attribute " + Util::toString(type));
	}
}

static const size_t TEMP_BUF_SIZE = 64 * 1024;

static bool addAttribValue(uint8_t outBuf[], size_t& ptr, uint8_t type, const void* data, size_t size)
{
	if (ptr + size + 3 > TEMP_BUF_SIZE || size > 32768) return false;
	outBuf[ptr++] = type;
	if (!type) return true;
	if (size < 128)
	{
		outBuf[ptr++] = (uint8_t) size;
	} else
	{
		outBuf[ptr++] = (uint8_t) ((size >> 8) | 0x80);
		outBuf[ptr++] = size & 0xFF;
	}
	memcpy(outBuf + ptr, data, size);
	ptr += size;
	return true;
}

#if 0
static bool addAttribValue32(uint8_t outBuf[], size_t& ptr, uint8_t type, uint32_t data)
{
	if (ptr + 6 > TEMP_BUF_SIZE) return false;
	outBuf[ptr] = type;
	outBuf[ptr + 1] = 4;
	storeUnaligned32(outBuf + ptr + 2, data);
	ptr += 6;
	return true;
}
#endif

static bool addAttribValue64(uint8_t outBuf[], size_t& ptr, uint8_t type, uint64_t data)
{
	if (ptr + 10 > TEMP_BUF_SIZE) return false;
	outBuf[ptr] = type;
	outBuf[ptr + 1] = 8;
	storeUnaligned64(outBuf + ptr + 2, data);
	ptr += 10;
	return true;
}

void ShareManager::writeShareDataDirStart(OutputStream* os, const SharedDir* dir, uint8_t tempBuf[])
{
	tempBuf[0] = SHARE_DATA_DIR_START;
	size_t ptr = 1;
	if (!addAttribValue(tempBuf, ptr, SHARE_DATA_ATTRIB_NAME, dir->name.data(), dir->name.length()) ||
	    !addAttribValue(tempBuf, ptr, 0, nullptr, 0))
		throw ShareWriterException("Can't write directory attributes");
	os->write(tempBuf, ptr);
}

void ShareManager::writeShareDataFile(OutputStream* os, const SharedFilePtr& file, uint8_t tempBuf[])
{
	tempBuf[0] = SHARE_DATA_FILE;
	size_t ptr = 1;
	if (!addAttribValue(tempBuf, ptr, SHARE_DATA_ATTRIB_NAME, file->name.data(), file->name.length()) ||
	    !addAttribValue64(tempBuf, ptr, SHARE_DATA_ATTRIB_SIZE, file->size) ||
	    !addAttribValue(tempBuf, ptr, SHARE_DATA_ATTRIB_TTH, file->tth.data, TTHValue::BYTES) ||
	    !addAttribValue64(tempBuf, ptr, SHARE_DATA_ATTRIB_TIMESTAMP, file->timestamp) ||
	    (file->timeShared && !addAttribValue64(tempBuf, ptr, SHARE_DATA_ATTRIB_TIME_SHARED, file->timeShared)) ||
	    !addAttribValue(tempBuf, ptr, 0, nullptr, 0))
		throw ShareWriterException("Can't write file attributes");
	os->write(tempBuf, ptr);
}

void ShareManager::writeShareDataDirEnd(OutputStream* os)
{
	uint8_t b = SHARE_DATA_DIR_END;
	os->write(&b, 1);
}

static inline bool isSubDir(const string& dir, const string& parent)
{
	dcassert(!dir.empty() && dir.back() == PATH_SEPARATOR);
	return parent.length() < dir.length() &&
		strnicmp(parent, dir, parent.length()) == 0;
}

static inline bool isSubDirOrSame(const string& dir, const string& parent)
{
	dcassert(!dir.empty() && dir.back() == PATH_SEPARATOR);
	return parent.length() <= dir.length() &&
		strnicmp(parent, dir, parent.length()) == 0;
}

#ifdef _WIN32
struct SystemPathInfo
{
	Util::SysPaths pathId;
	const char* description; // FIXME: Not used
};

static const SystemPathInfo systemPaths[] =
{
	{ Util::WINDOWS,          "CSIDL_WINDOWS"    },
	{ Util::PROGRAM_FILES,    "PROGRAM_FILES"    },
	{ Util::PROGRAM_FILESX86, "PROGRAM_FILESX86" },
	{ Util::APPDATA,          "APPDATA"          },
	{ Util::LOCAL_APPDATA,    "LOCAL_APPDATA"    },
	{ Util::COMMON_APPDATA,   "COMMON_APPDATA"   },
};
#endif

void ShareManager::addDirectory(const string& realPath, const string& virtualName)
{
	if (realPath.empty() || virtualName.empty())
		throw ShareException(STRING(NO_DIRECTORY_SPECIFIED), realPath);
	
	dcassert(realPath.back() == PATH_SEPARATOR);
	
	if (!File::isExist(realPath))
		throw ShareException(STRING(DIRECTORY_NOT_EXIST), realPath);
	
	string realPathNoSlash = realPath;
	Util::removePathSeparator(realPathNoSlash);
	
	FileFindIter fi(realPathNoSlash);
	if (!BOOLSETTING(SHARE_HIDDEN) && fi->isHidden())
		throw ShareException(STRING(DIRECTORY_IS_HIDDEN), realPathNoSlash);

	if (!BOOLSETTING(SHARE_SYSTEM) && fi->isSystem())
		throw ShareException(STRING(DIRECTORY_IS_SYSTEM), realPathNoSlash);
	
	if (!BOOLSETTING(SHARE_VIRTUAL) && fi->isVirtual())
		throw ShareException(STRING(DIRECTORY_IS_VIRTUAL), realPathNoSlash);

	const string tempDownloadDir = SETTING(TEMP_DOWNLOAD_DIRECTORY);
	if (stricmp(tempDownloadDir, realPath) == 0)
		throw ShareException(STRING(DONT_SHARE_TEMP_DIRECTORY), realPathNoSlash);
	
#ifdef _WIN32
	for (size_t i = 0; i < _countof(systemPaths); ++i)
	{
		if (Util::locatedInSysPath(systemPaths[i].pathId, realPath))
			throw ShareException(STRING_F(DONT_SHARE_SYSTEM_FOLDER, realPathNoSlash), realPathNoSlash);
	}
#endif
	
	{
		list<string> removeList;
		WRITE_LOCK(*csShare);
		bool virtualFound;
		if (hasShareL(virtualName, realPath, virtualFound))
		{
			if (virtualFound)
				throw ShareException(STRING_F(SHARE_ALREADY_EXISTS, virtualName), realPathNoSlash);
			else
				throw ShareException(STRING_F(FOLDER_ALREADY_SHARED, realPathNoSlash), realPathNoSlash);
		}
		for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		{
			if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
			const string& sharedName = i->realPath.getName();
			if (isSubDir(realPath, sharedName))
				throw ShareException(STRING_F(FOLDER_ALREADY_SHARED, realPathNoSlash), realPathNoSlash);
			if (isSubDir(sharedName, realPath))
				removeList.push_back(sharedName);
		}
		if (!removeList.empty())
		{
			for (auto i = shares.begin(); i != shares.end(); ++i)
			{
				if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
				const string& sharedName = i->realPath.getName();
				bool found = false;
				for (auto j = removeList.begin(); j != removeList.end(); ++j)
				{
					if (sharedName == *j)
					{
						found = true;
						removeList.erase(j);
						break;
					}
				}
				if (found)
				{
					i->dir->flags |= BaseDirItem::FLAG_SHARE_REMOVED;
					if (removeList.empty()) break;
				}
			}
		}
		ShareListItem sli;
		sli.realPath.setName(realPath);
		sli.dir = new SharedDir(virtualName, nullptr);
		sli.dir->flags |= BaseDirItem::FLAG_SIZE_UNKNOWN;
		sli.version = ++versionCounter;
		sli.totalFiles = 0;
		sli.flags = 0;
		bloom.add(sli.dir->getLowerName());
		shares.push_back(sli);
		shareListChanged = fileListChanged = true;
		++shareListVersion;
	}
}

void ShareManager::removeDirectory(const string& realPath)
{
	if (realPath.empty())
		return;

	dcassert(realPath.back() == PATH_SEPARATOR);

	HashManager::getInstance()->stopHashing(realPath);

	string pathLower;
	Text::toLower(realPath, pathLower);
	bool found = false;	
	WRITE_LOCK(*csShare);
	for (ShareListItem& sli : shares)
		if (!(sli.dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) && sli.realPath.getLowerName() == pathLower)
		{
			sli.dir->flags |= BaseDirItem::FLAG_SHARE_REMOVED;
			found = true;
			break;
		}

	if (!found) return;

	// Remove it from each share group
	for (auto& i : shareGroups)
	{
		auto& shareList = i.second.shares;
		for (auto j = shareList.begin(); j != shareList.end(); ++j)
			if (j->getLowerName() == pathLower)
			{
				shareList.erase(j);
				break;
			}
	}

	// Remove corresponding excludes
	for (auto j = notShared.cbegin(); j != notShared.cend();)
	{
		if (isSubDirOrSame(*j, realPath))
			j = notShared.erase(j);
		else
			++j;
	}

	shareListChanged = fileListChanged = true;
	++shareListVersion;
}

void ShareManager::renameDirectory(const string& realPath, const string& virtualName)
{
	string pathLower, virtualLower;
	Text::toLower(realPath, pathLower);
	Text::toLower(virtualName, virtualLower);
	SharedDir* dir = nullptr;
	WRITE_LOCK(*csShare);
	for (auto i = shares.begin(); i != shares.end(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		if (i->realPath.getLowerName() == pathLower)
			dir = i->dir;
		else if (i->dir->lowerName == virtualLower)
		{
			string realPathNoSlash = i->realPath.getName();
			Util::removePathSeparator(realPathNoSlash);
			throw ShareException(STRING_F(SHARE_ALREADY_EXISTS, virtualName), realPathNoSlash);
		}
	}
	if (!dir || virtualName == dir->name) return;
	dir->setName(virtualName);
	updateBloomL();
	fileListChanged = true;
	++shareListVersion;
}

bool ShareManager::addExcludeFolder(const string& path)
{
	dcassert(!path.empty() && path.back() == PATH_SEPARATOR);

	HashManager::getInstance()->stopHashing(path);
	
	WRITE_LOCK(*csShare);
	if (!addExcludeFolderL(path)) return false;
	shareListChanged = fileListChanged = true;
	++shareListVersion;
	return true;
}

bool ShareManager::addExcludeFolderL(const string& path) noexcept
{
	// make sure this is a sub folder of a shared folder
	auto itShare = shares.end();
	for (auto i = shares.begin(); i != shares.end(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		const string& sharedPath = i->realPath.getName();
		if (isSubDirOrSame(path, sharedPath))
		{
			if (path.length() == sharedPath.length()) return false;
			itShare = i;
			break;
		}
	}
	
	if (itShare == shares.end())
		return false;
		
	// Make sure this is not a subfolder of an already excluded folder
	for (auto j = notShared.cbegin(); j != notShared.cend(); ++j)
	{
		if (isSubDirOrSame(path, *j))
			return false;
	}
	
	// remove all sub folder excludes
	for (auto j = notShared.cbegin(); j != notShared.cend();)
	{
		if (isSubDir(*j, path))
			j = notShared.erase(j);
		else
			++j;
	}

	// update version
	itShare->version = ++versionCounter;
	itShare->dir->flags |= BaseDirItem::FLAG_SIZE_UNKNOWN;
	
	// add it to the list
	notShared.push_back(path);
	return true;
}

bool ShareManager::removeExcludeFolder(const string& path) noexcept
{
	WRITE_LOCK(*csShare);
	bool result = false;
	for (auto j = notShared.cbegin(); j != notShared.cend();)
	{
		const string& excludedPath = *j;
		if (excludedPath.length() == path.length() &&
		    strnicmp(path, excludedPath, path.length()) == 0)
		{
			notShared.erase(j);
			result = true;
			break;
		}
		else
			++j;
	}
	if (!result)
		return false;

	for (auto i = shares.begin(); i != shares.end(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		const string& sharedPath = i->realPath.getName();
		if (isSubDirOrSame(path, sharedPath))
		{
			i->version = ++versionCounter;
			i->dir->flags |= BaseDirItem::FLAG_SIZE_UNKNOWN;
			break;
		}
	}
	shareListChanged = fileListChanged = true;
	++shareListVersion;
	return true;
}

bool ShareManager::isDirectoryShared(const string& path) const noexcept
{
	dcassert(!path.empty() && path.back() == PATH_SEPARATOR);

	READ_LOCK(*csShare);
	bool result = false;
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		const string& sharedPath = i->realPath.getName();
		if (path.length() == sharedPath.length() && stricmp(path, sharedPath) == 0)
			return true;
		if (isSubDir(path, sharedPath))
		{
			result = true;
			break;
		}
	}
	if (!result)
		return false;
	
	// check if it's an excluded folder or a sub folder of an excluded folder
	for (auto j = notShared.cbegin(); j != notShared.cend(); ++j)
	{
		if (isSubDirOrSame(path, *j))
			return false;
	}
	return true;
}

void ShareManager::addShareGroupL(const string& name, const CID& id, const list<string>& shareList, const FileAttr* attr)
{
	if (name.empty() && !id.isZero())
		throw ShareException(STRING(SHARE_GROUP_EMPTY_NAME));
	for (const auto& i : shareGroups)
	{
		const ShareGroup& sg = i.second;
		if (sg.id == id)
			throw ShareException(STRING_F(SHARE_GROUP_ALREADY_EXISTS, id.toBase32()));
		if (!stricmp(name, sg.name))
			throw ShareException(STRING_F(SHARE_GROUP_ALREADY_EXISTS, name));
	}
	ShareGroup sg;
	sg.id = id;
	sg.name = name;	
	if (attr && attr[FILE_ATTR_FILES_BZ_XML].size >= 0 && attr[FILE_ATTR_FILES_XML].size >= 0)
	{
		sg.attrUncomp = attr[FILE_ATTR_FILES_XML];
		sg.attrComp = attr[FILE_ATTR_FILES_BZ_XML];
	}
	BaseDirItem item;
	for (const string& path : shareList)
	{
		item.setName(path);
		sg.shares.push_back(item);
	}
	shareGroups.insert(make_pair(id, sg));
}

void ShareManager::addShareGroup(const string& name, const list<string>& shareList, CID& outId)
{
	WRITE_LOCK(*csShare);
	while (true)
	{
		outId.regenerate();
		if (shareGroups.find(outId) == shareGroups.end()) break;
	}
	addShareGroupL(name, outId, shareList, nullptr);
	fileListChanged = true;
}

void ShareManager::removeShareGroup(const CID& id) noexcept
{
	if (id.isZero()) return;
	{
		WRITE_LOCK(*csShare);
		auto i = shareGroups.find(id);
		if (i == shareGroups.end()) return;
		shareGroups.erase(i);
		fileListChanged = true;
	}
	removeShareGroupFiles(Util::getConfigPath() + "ShareGroups" PATH_SEPARATOR_STR + id.toBase32() + PATH_SEPARATOR);
}

void ShareManager::updateShareGroup(const CID& id, const string& name, const list<string>& shareList)
{
	WRITE_LOCK(*csShare);
	auto i = shareGroups.find(id);
	if (i == shareGroups.end()) return;
	if (!id.isZero() && name != i->second.name)
	{
		for (const auto& j : shareGroups)
		{
			const ShareGroup& sg = j.second;
			if (sg.id == id) continue;
			if (!stricmp(sg.name, name))
				throw ShareException(STRING_F(SHARE_GROUP_ALREADY_EXISTS, name));
		}
	}
	ShareGroup& sg = i->second;
	sg.name = name;
	sg.shares.clear();
	BaseDirItem item;
	for (const string& path : shareList)
	{
		item.setName(path);
		sg.shares.push_back(item);
	}
	fileListChanged = true;
}

void ShareManager::removeShareGroupFiles(const string& path) noexcept
{
	StringList files = File::findFiles(path, "files*.xml.bz2");
	for (const string& file : files)
		File::deleteFile(file);
	files = File::findFiles(path, "*.tmp");
	for (const string& file : files)
		File::deleteFile(file);
	File::deleteFile(path + fileAttrXml);
	File::removeDirectory(path);
}

void ShareManager::removeOldShareGroupFiles() noexcept
{
	string path = Util::getConfigPath() + "ShareGroups" PATH_SEPARATOR_STR;
	StringList files = File::findFiles(path, "*", false);
	READ_LOCK(*csShare);
	for (const string& file : files)
	{
		if (!(file.length() == 40 && file.back() == PATH_SEPARATOR)) continue;
		CID id(file);
		if (shareGroups.find(id) == shareGroups.cend())
			removeShareGroupFiles(path + file);
	}
}

void ShareManager::addFile(const string& path, const TTHValue& root)
{
	string::size_type pos = path.rfind(PATH_SEPARATOR);
	if (pos == string::npos || pos == path.length()-1)
		throw ShareException(STRING(NO_DIRECTORY_SPECIFIED), path);

	uint64_t timestamp;
	int64_t size;
	{
		FileFindIter fileIter(path);
		if (fileIter == FileFindIter::end)
			throw ShareException(STRING(FILE_NOT_EXIST), path);

		timestamp = fileIter->getTimeStamp();
		size = fileIter->getSize();
	}
	
	string fileName = path.substr(pos + 1);
	string pathLower;
	Text::toLower(path, pathLower);
	uint16_t typesMask = getFileTypesFromFileName(fileName);
	
	WRITE_LOCK(*csShare);
	SharedDir* dir;
	string unused;
	if (!findByRealPathL(pathLower, dir, unused))
		throw ShareException(STRING(DIRECTORY_NOT_SHARED), path);

	uint64_t currentTime = Util::getFileTime();
	SharedFilePtr file = std::make_shared<SharedFile>(fileName, root, size, timestamp, currentTime, typesMask);
	if (!dir->files.insert(make_pair(file->getLowerName(), file)).second)
		throw ShareException(STRING(FILE_ALREADY_SHARED), path);

	dir->updateSize(size);
	dir->updateTypes(typesMask, 0);

	TTHMapItem tthItem;
	tthItem.file = file;
	tthItem.dir = dir;
	tthIndex.insert(make_pair(root, tthItem));

	bloom.add(file->getLowerName());
}

void ShareManager::saveShareList(SimpleXML& xml) const
{
	READ_LOCK(*csShare);
	
	xml.addTag("Share");
	xml.stepIn();
	for (const auto& sli : shares)
	{
		if (sli.flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		xml.addTag("Directory", sli.realPath.getName());
		xml.addChildAttrib("Virtual", sli.dir->name);
	}
	xml.stepOut();
	
	xml.addTag("NoShare");
	xml.stepIn();
	for (const auto& name : notShared)
	{
		xml.addTag("Directory", name);
	}
	xml.stepOut();

	xml.addTag("ShareGroups");
	xml.stepIn();
	for (const auto& i : shareGroups)
	{
		const ShareGroup& sg = i.second;
		xml.addTag("ShareGroup");
		if (!sg.id.isZero())
		{
			xml.addChildAttrib("ID", sg.id.toBase32());
			xml.addChildAttrib("Name", sg.name);
		}
		xml.stepIn();
		for (const BaseDirItem& share : sg.shares)
			xml.addTag("Directory", share.getName());
		xml.stepOut();
	}
	xml.stepOut();
}

ShareManager::ShareList::const_iterator ShareManager::getByVirtualL(const string& virtualName) const noexcept
{
	string virtualNameLower;
	Text::toLower(virtualName, virtualNameLower);
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		if (i->dir->getLowerName() == virtualNameLower)
			return i;
	}
	return shares.cend();
}

ShareManager::ShareList::const_iterator ShareManager::getByRealL(const string& realName) const noexcept
{
	string realNameLower;
	Text::toLower(realName, realNameLower);
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		if (i->realPath.getLowerName() == realNameLower)
			return i;
	}
	return shares.cend();
}

bool ShareManager::hasShareL(const string& virtualName, const string& realName, bool& foundVirtual) const noexcept
{
	string virtualNameLower, realNameLower;
	Text::toLower(virtualName, virtualNameLower);
	Text::toLower(realName, realNameLower);
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		if (i->realPath.getLowerName() == realNameLower)
		{
			foundVirtual = false;
			return true;
		}
		if (i->dir->getLowerName() == virtualNameLower)
		{
			foundVirtual = true;
			return true;
		}
	}
	return false;
}

bool ShareManager::parseVirtualPathL(const string& virtualPath, const SharedDir* &dir, string& filename, const ShareGroup& sg) const noexcept
{
	dir = nullptr;
	if (virtualPath.empty() || virtualPath[0] != '/')
		return false;
	
	string::size_type i = virtualPath.find('/', 1);
	if (i == string::npos || i == 1)
		return false;
	
	auto dmi = getByVirtualL(virtualPath.substr(1, i - 1));
	if (dmi == shares.cend())
		return false;
	
	if (!sg.hasShare(*dmi))
		return false;

	const SharedDir* d = dmi->dir;
	string::size_type j = i + 1;
	while ((i = virtualPath.find('/', j)) != string::npos)
	{
		string dirName = virtualPath.substr(j, i - j);
		Text::makeLower(dirName);
		auto mi = d->dirs.find(dirName);
		j = i + 1;
		if (mi == d->dirs.cend())
			return false;
		d = mi->second;
	}
	
	filename = virtualPath.substr(j);
	dir = d;
	return true;
}

bool ShareManager::parseVirtualPathL(const string& virtualPath, const SharedDir* &dir, SharedFilePtr& file, const ShareGroup& sg) const noexcept
{
	if (virtualPath.empty() || virtualPath.back() == '/')
		return false;
	string filename;
	if (!parseVirtualPathL(virtualPath, dir, filename, sg))
		return false;
	string filenameLower;
	Text::toLower(filename, filenameLower);
	auto it = dir->files.find(filenameLower);
	if (it == dir->files.cend())
		return false;
	file = it->second;
	return true;
}

bool ShareManager::isTTHShared(const TTHValue& tth) const noexcept
{
	READ_LOCK(*csShare);
	return tthIndex.find(tth) != tthIndex.end();
}

bool ShareManager::getFilePath(const TTHValue& tth, string& path, const CID& shareGroup) const noexcept
{
	READ_LOCK(*csShare);
	auto i = shareGroups.find(shareGroup);
	if (i == shareGroups.cend()) return false;
	auto p = tthIndex.equal_range(tth);
	if (p.first == p.second)
		return false;
	while (p.first != p.second)
	{
		const TTHMapItem& item = p.first->second;
		const ShareListItem* share;
		path = getFilePathL(item.dir, share);
		if (!path.empty() && i->second.hasShare(*share))
		{
			path += item.file->getName();
			return true;
		}
		++p.first;
	}
	path.clear();
	return false;
}

bool ShareManager::getFileInfo(const TTHValue& tth, string& path, int64_t& size) const noexcept
{
	READ_LOCK(*csShare);
	auto it = tthIndex.find(tth);
	if (it == tthIndex.end())
		return false;
	const TTHMapItem& item = it->second;
	const ShareListItem* share;
	path = getFilePathL(item.dir, share);
	if (!path.empty()) path += item.file->getName();
	size = item.file->getSize();
	return !path.empty();
}

bool ShareManager::getFileInfo(const TTHValue& tth, string& path) const noexcept
{
	READ_LOCK(*csShare);
	auto it = tthIndex.find(tth);
	if (it == tthIndex.end())
		return false;
	const TTHMapItem& item = it->second;
	const ShareListItem* share;
	path = getFilePathL(item.dir, share);
	if (!path.empty()) path += item.file->getName();
	return !path.empty();
}

bool ShareManager::getFileInfo(const TTHValue& tth, int64_t& size) const noexcept
{
	READ_LOCK(*csShare);
	auto it = tthIndex.find(tth);
	if (it == tthIndex.end())
		return false;
	size = it->second.file->getSize();
	return true;
}

bool ShareManager::getXmlFileInfo(const CID& id, bool compressed, TTHValue& tth, int64_t& size) const noexcept
{
	READ_LOCK(*csShare);
	auto i = shareGroups.find(id);
	if (i == shareGroups.end()) return false;
	const ShareGroup& sg = i->second;
	const FileAttr& fa = compressed ? sg.attrComp : sg.attrUncomp;
	tth = fa.root;
	size = fa.size;
	return true;
}

bool ShareManager::getFileInfo(AdcCommand& cmd, const string& filename, bool hideShare, const CID& shareGroup) const noexcept
{
	int fileAttrIndex = -1;
	if (filename == Transfer::fileNameFilesXml)
		fileAttrIndex = FILE_ATTR_EMPTY_FILES_XML;
	else if (filename == Transfer::fileNameFilesBzXml)
		fileAttrIndex = FILE_ATTR_EMPTY_FILES_BZ_XML;
	if (fileAttrIndex != -1)
	{
		if (hideShare)
		{
			const FileAttr& fa = fileAttr[fileAttrIndex];
			if (fa.size < 0) return false;
			cmd.addParam("FN", filename);
			cmd.addParam("SI", Util::toString(fa.size));
			cmd.addParam("TR", fa.root.toBase32());
			return true;
		}
		TTHValue tth;
		int64_t size = 0;
		bool isCompressed = fileAttrIndex == FILE_ATTR_EMPTY_FILES_BZ_XML;
		if (!getXmlFileInfo(CID(shareGroup), isCompressed, tth, size) && !getXmlFileInfo(CID(), isCompressed, tth, size))
			return false;
		cmd.addParam("FN", filename);
		cmd.addParam("SI", Util::toString(size));
		cmd.addParam("TR", tth.toBase32());
		return true;
	}

	if (hideShare)
		return false;

	if (filename.compare(0, 4, "TTH/", 4) != 0)
		return false;

	TTHValue tth;
	bool error;
	Encoder::fromBase32(filename.c_str() + 4, tth.data, sizeof(tth.data), &error);
	if (error) return false;
		
	READ_LOCK(*csShare);
	const auto i = tthIndex.find(tth);
	if (i == tthIndex.end())
		return false;
		
	const SharedDir* dir = i->second.dir;
	const SharedFilePtr& f = i->second.file;
	cmd.addParam("FN", getADCPathL(dir) + f->getName());
	cmd.addParam("SI", Util::toString(f->getSize()));
	cmd.addParam("TR", f->getTTH().toBase32());
	return true;
}

string ShareManager::getADCPathL(const SharedDir* dir) const noexcept
{
	string result;
	if (!dir) return result;
	while (dir->getParent())
	{
		result.insert(0, dir->getName() + '/');
		dir = dir->getParent();
	}
	result.insert(0, '/' + dir->getName() + '/');
	return result;
}

string ShareManager::getNMDCPathL(const SharedDir* dir) const noexcept
{
	string result;
	if (!dir) return result;
	do
	{
		result.insert(0, dir->getName() + '\\');
		dir = dir->getParent();
	} while (dir);
	return result;
}

string ShareManager::getNMDCPathL(const SharedDir* dir, const SharedDir* &topDir) const noexcept
{
	string result;
	if (!dir)
	{
		topDir = nullptr;
		return result;
	}
	const SharedDir* prev = nullptr;
	do
	{
		result.insert(0, dir->getName() + '\\');
		prev = dir;
		dir = dir->getParent();
	} while (dir);
	topDir = prev;
	return result;
}

string ShareManager::getFilePathL(const SharedDir* dir, const ShareListItem* &share) const noexcept
{
	string result;
	share = nullptr;
	if (!dir) return result;
	while (dir->getParent())
	{
		result.insert(0, dir->getName() + PATH_SEPARATOR);
		dir = dir->getParent();
	}
	for (const ShareListItem& sli : shares)
		if (sli.dir == dir)
		{
			share = &sli;
			result.insert(0, sli.realPath.getName());
			return result;			
		}
	return string();
}

bool ShareManager::findByRealPathL(const string& pathLower, SharedDir* &dir, string& filename) const noexcept
{
	string::size_type start = 0;
	dir = nullptr;
	for (auto it = shares.cbegin(); it != shares.cend(); ++it)
	{
		if (it->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		const string& name = it->realPath.getLowerName();
		if (pathLower.compare(0, name.length(), name) == 0)
		{
			dir = it->dir;
			start = name.length();
			break;
		}
	}
	if (!dir)
		return false;

	for (;;)
	{
		string::size_type end = pathLower.find(PATH_SEPARATOR, start);
		if (end == string::npos) break;
		if (end == start)
		{
			start++;
			continue;
		}
		auto it = dir->dirs.find(pathLower.substr(start, end-start));
		if (it == dir->dirs.cend()) return false;
		dir = it->second;
		start = end + 1;
	}
		
	filename = pathLower.substr(start);
	return true;
}

bool ShareManager::findByRealPathL(const string& pathLower, SharedDir* &dir, SharedFilePtr& file) const noexcept
{
	string filename;
	if (!findByRealPathL(pathLower, dir, filename))
		return false;
	
	auto it = dir->files.find(filename);
	if (it == dir->files.cend())
		return false;

	file = it->second;
	return true;
}

bool ShareManager::findByRealPath(const string& realPath, TTHValue* outTTH, string* outFilename, int64_t* outSize) const noexcept
{
	string pathLower;
	Text::toLower(realPath, pathLower);

	READ_LOCK(*csShare);
	SharedDir* dir;
	SharedFilePtr file;
	if (!findByRealPathL(pathLower, dir, file)) return false;
	if (outTTH) *outTTH = file->getTTH();
	if (outFilename) *outFilename = file->getName();
	if (outSize) *outSize = file->getSize();
	return true;
}

string ShareManager::getFileByPath(const string& virtualFile, bool hideShare, const CID& shareGroup, int64_t& xmlSize, string* errorText) const noexcept
{
	if (virtualFile == "MyList.DcLst")
	{
		if (errorText)
			*errorText = "NMDC-style lists no longer supported, please upgrade your client";
		return Util::emptyString;
	}
	if (virtualFile == Transfer::fileNameFilesBzXml || virtualFile == Transfer::fileNameFilesXml)
	{
		if (hideShare)
		{
			xmlSize = fileAttr[FILE_ATTR_EMPTY_FILES_XML].size;
			return getEmptyBZXmlFile();
		}
		string path = getBZXmlFile(shareGroup, xmlSize);
		if (path.empty())
		{
			path = getBZXmlFile(CID(), xmlSize);
			if (path.empty())
			{
				xmlSize = fileAttr[FILE_ATTR_EMPTY_FILES_XML].size;
				path = getEmptyBZXmlFile();
			}
		}
		return path;
	}
	{
		if (hideShare)
			return Util::emptyString;
		READ_LOCK(*csShare);
		auto i = shareGroups.find(shareGroup);
		if (i == shareGroups.end())
			return Util::emptyString;
		const SharedDir* dir;
		SharedFilePtr file;
		if (!parseVirtualPathL(virtualFile, dir, file, i->second))
			return Util::emptyString;
		const ShareListItem* unused;
		return getFilePathL(dir, unused) + file->getName();
	}
}

MemoryInputStream* ShareManager::getTreeFromStore(const TTHValue& tth) noexcept
{
	ByteVector buf;
	TigerTree tree;
	auto db = DatabaseManager::getInstance();
	auto hashDb = db->getHashDatabaseConnection();
	if (hashDb)
	{
		if (db->getTree(hashDb, tth, tree))
			tree.getLeafData(buf);
		db->putHashDatabaseConnection(hashDb);
	}
	if (buf.empty()) // If tree is not available, send only the root
		return new MemoryInputStream(tth.data, TTHValue::BYTES);
	return new MemoryInputStream(&buf[0], buf.size());
}

MemoryInputStream* ShareManager::getTreeByTTH(const TTHValue& tth) const noexcept
{
	if (!isTTHShared(tth)) return nullptr;
	return getTreeFromStore(tth);
}

MemoryInputStream* ShareManager::getTree(const string& virtualFile, const CID& shareGroup) const noexcept
{
	TTHValue tth;
	{
		READ_LOCK(*csShare);
		auto i = shareGroups.find(shareGroup);
		if (i == shareGroups.end())
			return nullptr;
		const SharedDir* dir;
		SharedFilePtr file;
		if (!parseVirtualPathL(virtualFile, dir, file, i->second))
			return nullptr;
		tth = file->getTTH();
	}
	return getTreeFromStore(tth);
}

void ShareManager::getHashBloom(ByteVector& v, size_t k, size_t m, size_t h) noexcept
{
	HashBloomCacheKey key;
	key.params[0] = k;
	key.params[1] = m;
	key.params[2] = h;
	csHashBloom.lock();
	const HashBloomCacheItem* item = hashBloom.get(key);
	if (item)
	{
		v.resize(item->size);
		memcpy(&v[0], item->data.get(), item->size);
		csHashBloom.unlock();
		return;
	}
	csHashBloom.unlock();

	dcdebug("Creating bloom filter, k=%u, m=%u, h=%u\n", unsigned(k), unsigned(m), unsigned(h));
	HashBloom bloom;
	bloom.reset(k, m, h);
	{
		READ_LOCK(*csShare);
		for (auto i = tthIndex.cbegin(); i != tthIndex.cend(); ++i)
			bloom.add(i->first);
	}
	bloom.copy_to(v);

	HashBloomCacheItem newItem;
	newItem.key = key;
	newItem.size = v.size();
	newItem.data.reset(new uint8_t[newItem.size]);
	memcpy(newItem.data.get(), &v[0], newItem.size);

	csHashBloom.lock();
	hashBloom.removeOldest(HASH_BLOOM_CACHE_SIZE);
	hashBloom.add(newItem);
	csHashBloom.unlock();
}

void ShareManager::writeShareDataL(const SharedDir* dir, OutputStream* shareDataFile, uint8_t tempBuf[]) const
{
	writeShareDataDirStart(shareDataFile, dir, tempBuf);
	for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
		writeShareDataL(i->second, shareDataFile, tempBuf);
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const auto& f = i->second;
		if (f->flags & BaseDirItem::FLAG_HASH_FILE)
			continue;
		writeShareDataFile(shareDataFile, f, tempBuf);
	}
	writeShareDataDirEnd(shareDataFile);
}

class BufferedTigerTreeHasher
{
	private:
		static const size_t BUF_SIZE = 64 * 1024;
		uint8_t buf[BUF_SIZE];
		size_t bufSize;
		TigerTree tree;

	public:
		BufferedTigerTreeHasher(): bufSize(0), tree(1ll<<40) {}

		void update(const void* data, size_t size)
		{
			if (bufSize)
			{
				if (bufSize + size <= BUF_SIZE)
				{
					memcpy(buf + bufSize, data, size);
					bufSize += size;
					if (bufSize == BUF_SIZE)
					{
						tree.update(buf, BUF_SIZE);
						bufSize = 0;
					}
					return;
				}
				size_t part = BUF_SIZE - bufSize;
				memcpy(buf + bufSize, data, part);
				tree.update(buf, BUF_SIZE);
				bufSize = 0;
				data = static_cast<const uint8_t*>(data) + part;
				size -= part;
			}
			while (size >= BUF_SIZE)
			{
				tree.update(data, BUF_SIZE);
				data = static_cast<const uint8_t*>(data) + BUF_SIZE;
				size -= BUF_SIZE;
			}
			memcpy(buf, data, size);
			bufSize = size;
		}

		void finalize(TTHValue& out)
		{
			if (bufSize) tree.update(buf, bufSize);
			tree.finalize();
			out = tree.getRoot();
		}
};

struct FileListFilter
{
	BZFilter bzipper;
	BufferedTigerTreeHasher treeOriginal;
	BufferedTigerTreeHasher treeCompressed;
	int64_t sizeOriginal = 0;
	int64_t sizeCompressed = 0;

	bool operator()(const void* in, size_t& insize, void* out, size_t& outsize)
	{
		bool result = bzipper(in, insize, out, outsize);
		treeOriginal.update(in, insize);
		treeCompressed.update(out, outsize);
		sizeOriginal += insize;
		sizeCompressed += outsize;
		return result;
	}
};

#define LITERAL(n) n, sizeof(n)-1

void ShareManager::writeXmlL(const SharedDir* dir, OutputStream& xmlFile, string& indent, string& tmp, int mode) const
{
	if (!indent.empty())
		xmlFile.write(indent);
	xmlFile.write(LITERAL("<Directory Name=\""));
	xmlFile.write(SimpleXML::escapeAttrib(dir->getName(), tmp));
	
#ifdef DEBUG_FILELIST
	xmlFile.write("\" FilesTypes=\"");
	xmlFile.write(Util::toString(dir->filesTypesMask));
	xmlFile.write("\" DirsTypes=\"");
	xmlFile.write(Util::toString(dir->dirsTypesMask));
	xmlFile.write("\" TotalSize=\"");
	xmlFile.write(Util::toString(dir->totalSize));
#endif

	if (mode == MODE_FULL_LIST ||
		(mode == MODE_RECURSIVE_PARTIAL_LIST && static_cast<StringOutputStream&>(xmlFile).getOutputSize() < MAX_PARTIAL_LIST_SIZE))
	{
		xmlFile.write(LITERAL("\">\r\n"));		
		indent += '\t';

		for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
			writeXmlL(i->second, xmlFile, indent, tmp, mode);

		writeXmlFilesL(dir, xmlFile, indent, tmp);

		indent.erase(indent.length() - 1);
		if (!indent.empty())
			xmlFile.write(indent);

		xmlFile.write(LITERAL("</Directory>\r\n"));
	}
	else
	{
		if (dir->dirs.empty() && dir->files.empty())
			xmlFile.write(LITERAL("\" />\r\n"));
		else
			xmlFile.write(LITERAL("\" Incomplete=\"1\" />\r\n"));
	}
}

void ShareManager::writeXmlFilesL(const SharedDir* dir, OutputStream& xmlFile, string& indent, string& tmp) const
{
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const auto& f = i->second;
		if (f->flags & BaseDirItem::FLAG_HASH_FILE)
			continue;
		if (!indent.empty())
			xmlFile.write(indent);
		xmlFile.write(LITERAL("<File Name=\""));
		xmlFile.write(SimpleXML::escapeAttrib(f->getName(), tmp));
		xmlFile.write(LITERAL("\" Size=\""));
		xmlFile.write(Util::toString(f->size));
		xmlFile.write(LITERAL("\" TTH=\""));
		tmp.clear();
		xmlFile.write(f->getTTH().toBase32(tmp));
		if (optionIncludeUploadCount && hashDb)
		{
			uint32_t uploadCount;
			unsigned unusedFlags;
			hashDb->getFileInfo(f->getTTH().data, unusedFlags, nullptr, nullptr, nullptr, &uploadCount);
			if (uploadCount)
			{
				xmlFile.write(LITERAL("\" HIT=\""));
				xmlFile.write(Util::toString(uploadCount));
			}
		}
		if (optionIncludeTimestamp && f->timeShared)
		{
			xmlFile.write(LITERAL("\" Shared=\""));
			xmlFile.write(Util::toString(f->timeShared));
		}
		//xmlFile.write(LITERAL("\" TS=\""));
		//xmlFile.write(Util::toString(f->getTS()));
		xmlFile.write(LITERAL("\"/>\r\n"));
	}
}

static void deleteTempFiles(const string& fullPath, const string& skipFile) noexcept
{
	string::size_type pos = fullPath.rfind(PATH_SEPARATOR);
	if (pos == string::npos) return;
	string directory = fullPath.substr(0, pos + 1);
	string oldFilename = fullPath.substr(pos + 1);
	pos = oldFilename.find('.');
	if (pos == string::npos) return;
	string pattern = oldFilename;
	pattern.insert(pos, 1, '*');
	StringList found = File::findFiles(directory, pattern, false);
	for (auto i = found.cbegin(); i != found.cend(); ++i)
	{
		const string& filename = *i;
		if (filename.back() != PATH_SEPARATOR &&
		    stricmp(filename, oldFilename) != 0 && stricmp(filename, skipFile) != 0)
				File::deleteFile(directory + filename);
	}
}

bool ShareManager::writeShareGroupXml(const CID& id)
{
	StringSet selectedShares;
	{
		READ_LOCK(*csShare);
		auto i = shareGroups.find(id);
		if (i == shareGroups.end()) return true;
		const ShareGroup& sg = i->second;
		for (const BaseDirItem& item : sg.shares)
			selectedShares.insert(item.getLowerName());
	}

	string tmp;
	string indent;
	string tempFileName = "files" + Util::toString(tempFileCount) + ".xml.bz2";
	string path = Util::getConfigPath();
	bool isDefault = id.isZero();
	if (!isDefault)
	{
		path += "ShareGroups" PATH_SEPARATOR_STR + id.toBase32() + PATH_SEPARATOR;
		File::ensureDirectory(path);
	}
	string origXmlName = path + fileBZXml;
	string newXmlName = path + tempFileName;

	FileAttr attr[MAX_FILE_ATTR];
	for (int i = 0; i < MAX_FILE_ATTR; ++i) attr[i].size = -1;
	bool result = false;

	{
		File outFileXml(newXmlName, File::WRITE, File::TRUNCATE | File::CREATE);
		FilteredOutputStream<FileListFilter, false> newXmlFile(&outFileXml);

		if (optionIncludeUploadCount && !hashDb)
			hashDb = DatabaseManager::getInstance()->getHashDatabaseConnection();

		newXmlFile.write(SimpleXML::utf8Header);
		newXmlFile.write(LITERAL("<FileListing Version=\"1\" CID=\""));
		newXmlFile.write(ClientManager::getMyCID().toBase32());
		newXmlFile.write(LITERAL("\" Base=\"/\" Generator=\"DC++ " DCVERSIONSTRING "\">\r\n"));
		{
			READ_LOCK(*csShare);
			for (const ShareListItem& sli : shares)
			{
				if (sli.flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
				if (selectedShares.find(sli.realPath.getLowerName()) == selectedShares.end()) continue;
				writeXmlL(sli.dir, newXmlFile, indent, tmp, MODE_FULL_LIST);
			}
		}
		newXmlFile.write(LITERAL("</FileListing>"));
		newXmlFile.flushBuffers(true);

		newXmlFile.getFilter().treeOriginal.finalize(attr[FILE_ATTR_FILES_XML].root);
		newXmlFile.getFilter().treeCompressed.finalize(attr[FILE_ATTR_FILES_BZ_XML].root);

		attr[FILE_ATTR_FILES_XML].size = newXmlFile.getFilter().sizeOriginal;
		attr[FILE_ATTR_FILES_BZ_XML].size = newXmlFile.getFilter().sizeCompressed;

		if (hashDb)
		{
			DatabaseManager::getInstance()->putHashDatabaseConnection(hashDb);
			hashDb = nullptr;
		}

#if defined DEBUG_FILELIST
		LogManager::message(origXmlName + " uncompressed size: " + Util::toString(attr[FILE_ATTR_FILES_XML].size), false);
		LogManager::message(origXmlName + " uncompressed TTH: " + attr[FILE_ATTR_FILES_XML].root.toBase32(), false);
		LogManager::message(origXmlName + " compressed size: " + Util::toString(attr[FILE_ATTR_FILES_BZ_XML].size), false);
		LogManager::message(origXmlName + " compressed TTH: " + attr[FILE_ATTR_FILES_BZ_XML].root.toBase32(), false);
#endif
	}

	{
		result = File::renameFile(newXmlName, origXmlName);
#if defined DEBUG_FILELIST
		if (!result) LogManager::message("Failed to rename " + newXmlName + " to " + origXmlName + ": " + Util::translateError(), false);
#endif
		deleteTempFiles(origXmlName, result ? Util::emptyString : tempFileName);

		WRITE_LOCK(*csShare);
		auto i = shareGroups.find(id);
		if (i == shareGroups.end()) return true;
		ShareGroup& sg = i->second;
		if (result) sg.tempXmlFile.clear(); else sg.tempXmlFile = std::move(tempFileName);
		sg.attrUncomp = attr[FILE_ATTR_FILES_XML];
		sg.attrComp = attr[FILE_ATTR_FILES_BZ_XML];
	}
	if (isDefault)
	{
		attr[FILE_ATTR_EMPTY_FILES_XML] = fileAttr[FILE_ATTR_EMPTY_FILES_XML];
		attr[FILE_ATTR_EMPTY_FILES_BZ_XML] = fileAttr[FILE_ATTR_EMPTY_FILES_BZ_XML];
	}
	writeFileAttr(path + fileAttrXml, attr);
	return result;
}

bool ShareManager::generateFileList(uint64_t tick) noexcept
{
	if (tick <= tickUpdateList)
		return false;
	
	bool prevStatus = false;
	if (!doingCreateFileList.compare_exchange_strong(prevStatus, true))
		return false;

	bool error = false;
	LogManager::message("Generating file list...", false);
	uint8_t tempBuf[TEMP_BUF_SIZE];

	optionIncludeUploadCount = BOOLSETTING(FILELIST_INCLUDE_UPLOAD_COUNT);
	optionIncludeTimestamp = BOOLSETTING(FILELIST_INCLUDE_TIMESTAMP);
	
	const string shareDataFileName = Util::getConfigPath() + fileShareData;
	string skipBZXmlFile;
	try
	{
		// Write Share.dat
		++tempFileCount;
		string newShareDataName = Util::getConfigPath() + "Share" + Util::toString(tempFileCount) + ".dat";
		{
			File outFileShareData(newShareDataName, File::WRITE, File::TRUNCATE | File::CREATE);
			BufferedOutputStream<false> newShareDataFile(&outFileShareData, 256 * 1024);
			READ_LOCK(*csShare);
			for (auto i = shares.cbegin(); i != shares.cend(); ++i)
				if (!(i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED))
					writeShareDataL(i->dir, &newShareDataFile, tempBuf);
			newShareDataFile.flushBuffers(true);
		}
		if (File::renameFile(newShareDataName, shareDataFileName))
			tempShareDataFile.clear();
		else
			tempShareDataFile = Util::getFileName(newShareDataName);

		boost::unordered_set<CID> updateGroups;
		{
			WRITE_LOCK(*csShare);
			for (auto& i : shareGroups)
			{
				ShareGroup& sg = i.second;
				if (updateShareGroupHashL(sg))
				{
					updateGroups.insert(sg.id);
					continue;
				}
				string path = Util::getConfigPath();
				if (!sg.id.isZero())
					path += "ShareGroups" PATH_SEPARATOR_STR + sg.id.toBase32() + PATH_SEPARATOR;
				if (sg.tempXmlFile.empty())
					path += fileBZXml;
				else
					path += sg.tempXmlFile;
				if (!File::isExist(path))
					updateGroups.insert(sg.id);
			}
			fileListChanged = false;
		}

		// Write files.xml.bz2
		bool result = true;
		for (const CID& id : updateGroups)
		{
#ifdef DEBUG_SHARE_MANAGER
			LogManager::message("Writing XML file: " + id.toBase32(), false);
#endif
			if (!writeShareGroupXml(id))
				result = false;
		}

		if (!result)
			tickRestoreFileList.store(GET_TICK() + 60000);
		tickUpdateList = std::numeric_limits<uint64_t>::max();
	}
	catch (const Exception& e)
	{
		LogManager::message("Error creating file list: " + e.getError());
		// Retry later
		tickUpdateList = GET_TICK() + 60000;
		error = true;
	}		

	deleteTempFiles(shareDataFileName, tempShareDataFile);

	doingCreateFileList.store(false);

	if (!error) LogManager:: message(STRING(FILE_LIST_UPDATED));
	return true;
}

MemoryInputStream* ShareManager::generatePartialList(const string& dir, bool recurse, bool hideShare, const CID& shareGroup) const
{
	if (dir.empty() || dir[0] != '/' || dir.back() != '/')
		return nullptr;

	optionIncludeUploadCount = BOOLSETTING(FILELIST_INCLUDE_UPLOAD_COUNT);
	optionIncludeTimestamp = BOOLSETTING(FILELIST_INCLUDE_TIMESTAMP);

	int mode = recurse ? MODE_RECURSIVE_PARTIAL_LIST : MODE_PARTIAL_LIST;
	string xml = SimpleXML::utf8Header;
	string tmp;
	xml += "<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"" + SimpleXML::escape(dir, tmp, false) + "\" Generator=\"DC++ "  DCVERSIONSTRING  "\">\r\n";
	if (hideShare)
	{
		xml += "</FileListing>";
		return new MemoryInputStream(xml);
	}
	StringOutputStream sos(xml);
	READ_LOCK(*csShare);
	
	string indent = "\t";
	if (dir == "/")
	{
		auto i = shareGroups.find(shareGroup);
		if (i == shareGroups.cend())
		{
			i = shareGroups.find(CID());
			if (i == shareGroups.cend())
			{
				xml += "</FileListing>";
				return new MemoryInputStream(xml);
			}
		}
		const auto& shareList = i->second.shares;
		for (const BaseDirItem& item : shareList)
			for (const ShareListItem& sli : shares)
			{
				if (sli.dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
				if (sli.realPath.getLowerName() == item.getLowerName())
				{
					tmp.clear();
					writeXmlL(sli.dir, sos, indent, tmp, mode);
					break;
				}
			}
	}
	else
	{
		string::size_type i = 1, j = 1;
		string lowerName;
		const SharedDir* root = nullptr;
		bool first = true;
		while ((i = dir.find('/', j)) != string::npos)
		{
			if (i == j)
			{
				j++;
				continue;
			}
			
			if (first)
			{
				first = false;
				auto it = getByVirtualL(dir.substr(j, i - j));				
				if (it == shares.cend())
					return nullptr;
				auto itShareGroup = shareGroups.find(shareGroup);
				if (itShareGroup == shareGroups.cend())
				{
					itShareGroup = shareGroups.find(CID());
					if (itShareGroup == shareGroups.cend())
						return nullptr;
				}
				if (!itShareGroup->second.hasShare(*it))
					return nullptr;
				root = it->dir;
			}
			else
			{
				Text::toLower(dir.substr(j, i - j), lowerName);
				auto it = root->dirs.find(lowerName);
				if (it == root->dirs.cend())
					return nullptr;
				root = it->second;
			}
			j = i + 1;
		}
		if (!root)
			return nullptr;
			
		for (auto it = root->dirs.cbegin(); it != root->dirs.cend(); ++it)
			writeXmlL(it->second, sos, indent, tmp, mode);
		writeXmlFilesL(root, sos, indent, tmp);
	}
	
	xml += "</FileListing>";
	
#if 0
	string debugMessage = "\n\n\n xml: [" + xml + "]\n";
	DumpDebugMessage(_T("generated-partial-list.log"), debugMessage.c_str(), debugMessage.length(), false);
#endif
	return new MemoryInputStream(xml);
}

void ShareManager::load(SimpleXML& xml)
{
	try
	{
		loadShareList(xml);
		string shareDataFile = Util::getConfigPath() + fileShareData;
		if (File::isExist(shareDataFile))
		{
			File file(shareDataFile, File::READ, File::OPEN);
			loadShareData(file);
		}
		else
		{
			string xmlFile = Util::getConfigPath() + fileBZXml;
			ShareLoader loader(*this);
			SimpleXMLReader reader(&loader);
			File file(xmlFile, File::READ, File::OPEN);
			FilteredInputStream<UnBZFilter, false> f(&file);
			reader.parse(f);
		}
	}
	catch (const FileException& e)
	{
		LogManager::message("Error reading share data file: " + e.getError(), false);
	}
	catch (const SimpleXMLException& e)
	{
		LogManager::message("Error parsing XML list: " + e.getError(), false);
	}
	catch (const ShareLoaderException& e)
	{
		LogManager::message("Error loading share data: " + e.getError(), false);
	}
}

void ShareManager::init()
{
	dcassert(ClientManager::isStartup());
	string xmlFile = Util::getConfigPath() + fileBZXml;
	updateSharedSizeL();
	initDefaultShareGroupL();
	if (!File::isExist(xmlFile))
	{
		File::copyFile(getEmptyBZXmlFile(), xmlFile);
		tickRefresh = 0;
		autoRefreshMode = REFRESH_MODE_FULL;
	}
	if (autoRefreshMode == REFRESH_MODE_FILE_LIST)
		tickUpdateList = 0;
	autoRefreshMode = REFRESH_MODE_NONE;
}

bool ShareManager::searchTTH(const TTHValue& tth, vector<SearchResultCore>& results, const Client* client, const CID& shareGroup) noexcept
{
	bool result = false;
	string name;
	csShare->acquireShared();
	auto i = shareGroups.find(shareGroup);
	if (i == shareGroups.cend())
	{
		csShare->releaseShared();
		return false;
	}
	auto p = tthIndex.equal_range(tth);
	while (p.first != p.second)
	{
		const auto& item = p.first->second;
		const SharedDir* topDir;
		name = getNMDCPathL(item.dir, topDir);
		if (topDir)
		{
			for (const ShareListItem& sli : shares)
				if (sli.dir == topDir)
				{
					result = i->second.hasShare(sli);
					break;
				}
		}
		if (result)
		{
			name += item.file->getName();
			results.emplace_back(SearchResult::TYPE_FILE, item.file->getSize(), name, item.file->getTTH());
			incHits();
			break;
		}
		++p.first;
	}
	csShare->releaseShared();

	if (result && CMD_DEBUG_ENABLED() && client)
		COMMAND_DEBUG("Search TTH=" + tth.toBase32() + " Found=" + name, DebugTask::HUB_IN, client->getIpPort());
	return result;
}

/**
 * Alright, the main point here is that when searching, a search string is most often found in
 * the filename, not directory name, so we want to make that case faster. Also, we want to
 * avoid changing StringLists unless we absolutely have to --> this should only be done if a string
 * has been matched in the directory name. This new stringlist should also be used in all descendants,
 * but not the parents...
 */
void ShareManager::searchL(const SharedDir* dir, vector<SearchResultCore>& results, const StringSearch::List& strings, const SearchParamBase& sp) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;

	// Skip everything if there's nothing to find here (doh! =)
	if (!dir->hasType(sp.fileType))
		return;
		
	const StringSearch::List* cur = &strings;
	unique_ptr<StringSearch::List> newStr;
	
	// Find any matches in the directory name
	for (auto k = strings.cbegin(); k != strings.cend(); ++k)
	{
		if (k->matchKeepCase(dir->getLowerName()))
		{
			if (!newStr.get())
				newStr = std::make_unique<StringSearch::List>(strings);
			newStr->erase(remove(newStr->begin(), newStr->end(), *k), newStr->end());
		}
	}
	
	if (newStr.get())
		cur = newStr.get();
	
	const bool sizeOk = sp.sizeMode != SIZE_ATLEAST || sp.size == 0;
	if (cur->empty() &&
	    ((sp.fileType == FILE_TYPE_ANY && sizeOk) || sp.fileType == FILE_TYPE_DIRECTORY))
	{
		// We satisfied all the search words! Add the directory...(NMDC searches don't support directory size)
		results.emplace_back(SearchResult::TYPE_DIRECTORY, 0, getNMDCPathL(dir), TTHValue());
		incHits();
	}
	
	if (sp.fileType != FILE_TYPE_DIRECTORY)
	{
		for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
		{
			const SharedFilePtr& file = i->second;
			if ((sp.sizeMode == SIZE_ATLEAST && file->getSize() < sp.size) ||
			    (sp.sizeMode == SIZE_ATMOST && file->getSize() > sp.size))
				continue;

			if (!file->hasType(sp.fileType))
				continue;

			const string& name = file->getLowerName();
			auto j = cur->cbegin();
			while (j != cur->cend() && j->matchKeepCase(name)) ++j;
			if (j != cur->cend())
				continue;
			
			results.emplace_back(SearchResult::TYPE_FILE, file->getSize(), getNMDCPathL(dir) + file->getName(), file->getTTH());
			incHits();
			if (results.size() >= sp.maxResults)
				return;
		}
	}
	if (sp.fileType == FILE_TYPE_ANY || (dir->dirsTypesMask & 1<<sp.fileType))
		for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
		{
			searchL(i->second, results, *cur, sp);
			if (results.size() >= sp.maxResults) break;
		}
}

void ShareManager::search(vector<SearchResultCore>& results, const NmdcSearchParam& sp, const Client* client) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	if (sp.fileType == FILE_TYPE_TTH)
	{
		if (Util::isTTHBase32(sp.filter))
		{
			const TTHValue tth(sp.filter.c_str() + 4);
			searchTTH(tth, results, client, sp.shareGroup);
		}
		return;
	}

	if (!sp.cacheKey.empty())
	{
		LOCK(csSearchCache);
		const CacheItem* item = searchCache.get(sp.cacheKey);
		if (item)
		{
			results = item->results;
			return;
		}
	}
	
	const StringTokenizer<string> t(Text::toLower(sp.filter), '$');
	const StringList& sl = t.getTokens();
	{
		bool bloomMatch;
		{
			READ_LOCK(*csShare);
			bloomMatch = bloom.match(sl);
		}
		if (!bloomMatch)
			return;
	}
	
	StringSearch::List ssl;
	ssl.reserve(sl.size());
	for (auto i = sl.cbegin(); i != sl.cend(); ++i)
	{
		if (!i->empty())
			ssl.push_back(StringSearch(*i));
	}
	if (!ssl.empty())
	{
		READ_LOCK(*csShare);
		auto i = shareGroups.find(sp.shareGroup);
		if (i == shareGroups.cend()) return;
		const auto& shareGroup = i->second;

		for (const ShareListItem& sli : shares)
		{
			if (sli.dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
			if (!shareGroup.hasShare(sli)) continue;
			searchL(sli.dir, results, ssl, sp);
			if (results.size() >= sp.maxResults) break;
		}
	}

	if (!sp.cacheKey.empty())
	{
		LOCK(csSearchCache);
		searchCache.removeOldest(SEARCH_CACHE_SIZE);
		CacheItem item;
		item.key = sp.cacheKey;
		item.results = results;
		searchCache.add(item);
	}
}

AdcSearchParam::AdcSearchParam(const StringList& params, unsigned maxResults, const CID& shareGroup) noexcept :
	shareGroup(shareGroup), gt(0), lt(std::numeric_limits<int64_t>::max()), hasRoot(false), isDirectory(false), maxResults(maxResults)
{
	for (auto i = params.cbegin(); i != params.cend(); ++i)
	{
		const string& p = *i;
		if (p.length() <= 2)
			continue;
		const uint16_t cmd = *reinterpret_cast<const uint16_t*>(p.data());
		if (TAG('T', 'R') == cmd)
		{
			hasRoot = true;
			root = TTHValue(p.substr(2));
			cacheKey.clear();
			return;
		}
		else if (TAG('A', 'N') == cmd)
		{
			include.push_back(StringSearch(p.substr(2)));
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('N', 'O') == cmd)
		{
			exclude.push_back(StringSearch(p.substr(2)));
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('E', 'X') == cmd)
		{
			exts.push_back(p.substr(2));
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('G', 'R') == cmd)
		{
			const auto extGroup = AdcHub::parseSearchExts(Util::toInt(p.substr(2)));
			exts.insert(exts.begin(), extGroup.begin(), extGroup.end());
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('R', 'X') == cmd)
		{
			noExts.push_back(p.substr(2));
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('G', 'E') == cmd)
		{
			gt = Util::toInt64(p.substr(2));
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('L', 'E') == cmd)
		{
			lt = Util::toInt64(p.substr(2));
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('E', 'Q') == cmd)
		{
			lt = gt = Util::toInt64(p.substr(2));
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('T', 'Y') == cmd)
		{
			isDirectory = p[2] == '2';
			cacheKey += ' ';
			cacheKey += p;
		}
		else if (TAG('T', 'O') == cmd)
		{
			token = p.substr(2);
		}
	}
	
	if (!cacheKey.empty())
	{
		if (!shareGroup.isZero())
		{
			cacheKey += "SG";
			cacheKey += shareGroup.toBase32();
		}
		cacheKey.insert(0, Util::toString(maxResults) + '=');
	}
}

bool AdcSearchParam::isExcluded(const string& strLower) const noexcept
{
	for (auto i = exclude.cbegin(); i != exclude.cend(); ++i)
		if (i->matchKeepCase(strLower)) return true;
	return false;
}

bool AdcSearchParam::hasExt(const string& name) noexcept
{
	if (exts.empty())
		return true;
	if (!noExts.empty())
	{
		exts = StringList(exts.begin(), set_difference(exts.begin(), exts.end(), noExts.begin(), noExts.end(), exts.begin()));
		noExts.clear();
	}
	for (auto i = exts.cbegin(); i != exts.cend(); ++i)
		if (name.length() >= i->length() && stricmp(name.c_str() + name.length() - i->length(), i->c_str()) == 0)
			return true;
	return false;
}

string AdcSearchParam::getDescription() const noexcept
{
	if (hasRoot)
		return "TTH:" + root.toBase32();
	string result;
	for (const auto& s : include)
	{
		if (!result.empty()) result += ' ';
		result += s.getPattern();
	}
	for (const auto& s : exclude)
	{
		if (!result.empty()) result += ' ';
		result += '-';
		result += s.getPattern();
	}
	if (result.empty()) result = '?';
	return result;
}

// ADC search
void ShareManager::searchL(const SharedDir* dir, vector<SearchResultCore>& results, AdcSearchParam& sp, const StringSearch::List* replaceInclude) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
		
	const StringSearch::List* cur = replaceInclude ? replaceInclude : &sp.include;
	unique_ptr<StringSearch::List> newStr;
	
	// Find any matches in the directory name
	for (auto k = cur->cbegin(); k != cur->cend(); ++k)
	{
		if (k->matchKeepCase(dir->getLowerName()) && !sp.isExcluded(dir->getLowerName()))
		{
			if (!newStr.get())
				newStr = std::make_unique<StringSearch::List>(*cur);
			newStr->erase(remove(newStr->begin(), newStr->end(), *k), newStr->end());
		}
	}
	
	if (newStr.get())
		cur = newStr.get();
	
	const bool sizeOk = sp.gt == 0;
	if (cur->empty() && sp.exts.empty() && sizeOk)
	{
		// We satisfied all the search words! Add the directory...
		results.emplace_back(SearchResult::TYPE_DIRECTORY, dir->totalSize, getNMDCPathL(dir), TTHValue());
		incHits();
	}
	
	if (!sp.isDirectory)
	{
		for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
		{
			const SharedFilePtr& file = i->second;
			if (file->getSize() < sp.gt || file->getSize() > sp.lt) continue;
			
			if (sp.isExcluded(file->getLowerName()))
				continue;
				
			// Check file type...
			const string& name = file->getLowerName();
			if (!sp.hasExt(name))
				continue;

			auto j = cur->cbegin();
			while (j != cur->cend() && j->matchKeepCase(name)) ++j;
			if (j != cur->cend())
				continue;

			results.emplace_back(SearchResult::TYPE_FILE, file->getSize(), getNMDCPathL(dir) + file->getName(), file->getTTH());
			incHits();
			if (results.size() >= sp.maxResults)
				return;
		}
	}	
	for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
	{
		searchL(i->second, results, sp, newStr.get());
		if (results.size() >= sp.maxResults) break;
	}
}

// ADC search
void ShareManager::search(vector<SearchResultCore>& results, AdcSearchParam& sp) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
		
	if (sp.hasRoot)
	{
		searchTTH(sp.root, results, nullptr, sp.shareGroup);
		return;
	}
	
	if (!sp.cacheKey.empty())
	{
		LOCK(csSearchCache);
		const CacheItem* item = searchCache.get(sp.cacheKey);
		if (item)
		{
			results = item->results;
			return;
		}
	}

	{
		READ_LOCK(*csShare);
		for (auto i = sp.include.cbegin(); i != sp.include.cend(); ++i)
			if (!bloom.match(i->getPattern()))
				return;

		auto j = shareGroups.find(sp.shareGroup);
		if (j == shareGroups.cend()) return;
		const auto& shareGroup = j->second;

		for (const ShareListItem& sli : shares)
		{
			if (sli.dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
			if (!shareGroup.hasShare(sli)) continue;
			searchL(sli.dir, results, sp, nullptr);
			if (results.size() >= sp.maxResults) break;
		}
	}

	if (!sp.cacheKey.empty())
	{
		LOCK(csSearchCache);
		searchCache.removeOldest(SEARCH_CACHE_SIZE);
		CacheItem item;
		item.key = sp.cacheKey;
		item.results = results;
		searchCache.add(item);
	}
}

#ifdef _DEBUG
bool ShareManager::matchBloom(const string& s) const noexcept
{
	READ_LOCK(*csShare);
	return bloom.match(s);
}

void ShareManager::getBloomInfo(size_t& size, size_t& used) const noexcept
{
	READ_LOCK(*csShare);
	bloom.getInfo(size, used);
}
#endif

bool ShareManager::isDirectoryExcludedL(const string& path) const noexcept
{
	for (auto j = newNotShared.cbegin(); j != newNotShared.cend(); ++j)
		if (isSubDirOrSame(path, *j)) return true;
	return false;
}

void ShareManager::scanDir(SharedDir* dir, const string& path)
{
	scanProgress[0]++;
	int64_t deltaSize = 0;
	uint16_t filesTypesMask = 0;
	uint16_t dirsTypesMask = 0;
	size_t countFiles = dir->files.size();
	size_t countDirs = dir->dirs.size();
	size_t foundFiles = 0;
	size_t foundDirs = 0;
	for (auto i = dir->dirs.begin(); i != dir->dirs.end(); ++i)
		i->second->flags |= BaseDirItem::FLAG_NOT_FOUND;
	for (auto i = dir->files.begin(); i != dir->files.end(); ++i)
		i->second->flags |= BaseDirItem::FLAG_NOT_FOUND;

	string lowerName;
	for (FileFindIter i(path + '*'); i != FileFindIter::end; ++i)
	{
		if (stopScanning) break;
		const string& fileName = i->getFileName();
		if (Util::isReservedDirName(fileName) || fileName.empty())
			continue;
		if (i->isTemporary())
			continue;
		if (i->isHidden() && !optionShareHidden)
			continue;
		if (i->isSystem() && !optionShareSystem)
			continue;
		if (i->isVirtual() && !optionShareVirtual)
			continue;
		Text::toLower(fileName, lowerName);
		if (i->isDirectory())
		{
			const string fullPath = path + fileName + PATH_SEPARATOR;
			if (Util::locatedInSysPath(fullPath))
				continue;
				
			if (stricmp(fullPath, SETTING(TEMP_DOWNLOAD_DIRECTORY)) == 0 ||
			    stricmp(fullPath, Util::getConfigPath()) == 0 ||
			    stricmp(fullPath, SETTING(LOG_DIRECTORY)) == 0 ||
			    isDirectoryExcludedL(fullPath)) continue;

			SharedDir* subdir;
			auto itDir = dir->dirs.find(lowerName);
			if (itDir != dir->dirs.end())
			{
				subdir = itDir->second;
				subdir->flags &= ~BaseDirItem::FLAG_NOT_FOUND;
				foundDirs++;
			}
			else
			{
				subdir = new SharedDir(fileName, dir);
				dir->dirs.insert(make_pair(lowerName, subdir));
				if (!(scanShareFlags & SCAN_SHARE_FLAG_REBUILD_BLOOM))
					bloomNew.add(dir->getLowerName());
				scanShareFlags |= SCAN_SHARE_FLAG_ADDED;
#ifdef DEBUG_SHARE_MANAGER
				LogManager::message("New directory shared: " + fullPath, false);
#endif
			}
			scanDir(subdir, fullPath);
			dirsTypesMask |= subdir->getTypes();
		}
		else
		{
			// Not a directory, assume it's a file...make sure we're not sharing the settings file...
			string ext = Util::getFileExtWithoutDot(lowerName);
			const string fullPath = path + fileName;
			int64_t size = i->getSize();
			if (isInSkipList(lowerName))
			{
				// !qb, jc!, ob!, dmf, mta, dmfr, !ut, !bt, bc!, getright, antifrag, pusd, dusd, download, crdownload
				string pathStr = Util::ellipsizePath(fullPath);
				string sizeStr = Util::formatBytes(size);
				LogManager::message(STRING_F(SKIPPING_FILE, pathStr % sizeStr));
				continue;
			}
#ifdef _WIN32
			if (i->isLink() && size == 0) // https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/14
			{
				try
				{
					File f(fullPath, File::READ, File::OPEN | File::SHARED);
					size = f.getSize();
				}
				catch (FileException&)
				{
					continue;
				}
			}
#endif
			fileCounter++;
			scanProgress[1]++;
			auto itFile = dir->files.find(lowerName);
			const uint64_t timestamp = i->getTimeStamp();
			int64_t oldSize = 0;
			if (itFile != dir->files.end())
			{
				foundFiles++;
				SharedFilePtr& file = itFile->second;
				filesTypesMask |= file->getFileTypes();
				oldSize = file->size;
				if (oldSize == size && file->timestamp == timestamp)
				{
					file->flags &= ~BaseDirItem::FLAG_NOT_FOUND;
					TTHMapItem tthItem;
					tthItem.file = file;
					tthItem.dir = dir;
					tthIndexNew.insert(make_pair(file->getTTH(), tthItem));
					continue;
				}
			}

			uint16_t types = getFileTypesFromFileName(fileName);
			SharedFilePtr newFile = std::make_shared<SharedFile>(fileName, lowerName, size, timestamp, types);
			filesTypesMask |= types;
			newFile->flags |= BaseDirItem::FLAG_HASH_FILE;
			dir->files.insert_or_assign(lowerName, newFile);
			deltaSize += newFile->getSize() - oldSize;
			if (!(scanShareFlags & SCAN_SHARE_FLAG_REBUILD_BLOOM))
				bloomNew.add(newFile->getLowerName());
#ifdef DEBUG_SHARE_MANAGER
			LogManager::message("New file: " + fullPath, false);
#endif
			filesToHash.emplace_back(FileToHash{newFile, fullPath});
			scanShareFlags |= SCAN_SHARE_FLAG_ADDED;
		}
	}

	if (foundFiles < countFiles)
	{
		auto i = dir->files.begin();
		while (i != dir->files.end())
		{
			SharedFilePtr& file = i->second;
			if (file->flags & BaseDirItem::FLAG_NOT_FOUND)
			{
				string fullPath = path + file->getName();
				deltaSize -= file->getSize();
#ifdef DEBUG_SHARE_MANAGER
				LogManager::message("File removed: " + fullPath, false);
#endif
				i = dir->files.erase(i);
				scanShareFlags |= SCAN_SHARE_FLAG_REMOVED | SCAN_SHARE_FLAG_REBUILD_BLOOM;
			} else ++i;
		}
	}
	if (foundDirs < countDirs)
	{
		auto i = dir->dirs.begin();
		while (i != dir->dirs.end())
		{
			SharedDir* d = i->second;
			if (d->flags & BaseDirItem::FLAG_NOT_FOUND)
			{
				string fullPath = path + d->getName();
				deltaSize -= d->totalSize;
#ifdef DEBUG_SHARE_MANAGER
				LogManager::message("Directory removed: " + fullPath, false);
#endif
				SharedDir::deleteTree(d);
				i = dir->dirs.erase(i);
				scanShareFlags |= SCAN_SHARE_FLAG_REMOVED | SCAN_SHARE_FLAG_REBUILD_BLOOM;
			} else ++i;
		}
	}
	if (deltaSize)
		dir->updateSize(deltaSize);
	if (scanShareFlags & SCAN_SHARE_FLAG_REMOVED)
		dir->updateTypes(filesTypesMask, dirsTypesMask);
	else
		dir->addTypes(filesTypesMask, dirsTypesMask);
}

void ShareManager::scanDirs()
{
	uint64_t startTick = GET_TICK();
	LogManager::message(STRING(FILE_LIST_REFRESH_INITIATED));

	optionShareHidden = BOOLSETTING(SHARE_HIDDEN);
	optionShareSystem = BOOLSETTING(SHARE_SYSTEM);
	optionShareVirtual = BOOLSETTING(SHARE_VIRTUAL);

	scanProgress[0] = scanProgress[1] = 0;

	ShareList newShares;
	ShareListItem sli;
	sli.totalFiles = 0;
	sli.flags = 0;
	{
		READ_LOCK(*csShare);
		scanAllFlags = 0;
#ifdef DEBUG_SHARE_MANAGER
		uint64_t tsStart = GET_TICK();
#endif
		for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		{
			if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED)
			{
				scanAllFlags |= SCAN_SHARE_FLAG_REMOVED | SCAN_SHARE_FLAG_REBUILD_BLOOM;
				continue;
			}
			sli.dir = SharedDir::copyTree(i->dir);
			sli.realPath = i->realPath;
			sli.version = i->version;
			newShares.push_back(sli);
		}
		newNotShared = notShared;
#ifdef DEBUG_SHARE_MANAGER
		uint64_t tsEnd = GET_TICK();
		LogManager::message("Copying directory tree: " + Util::toString(tsEnd-tsStart), false);
#endif
		bloomNew = bloom;
	}

	tthIndexNew.clear();
	filesToHash.clear();

	rebuildSkipList();
	for (ShareListItem& sli : newShares)
	{
		if (stopScanning) break;
		const string& path = sli.realPath.getName();
		LogManager::message("Scanning share: " + path, false);
		scanShareFlags = scanAllFlags & SCAN_SHARE_FLAG_REBUILD_BLOOM;
		fileCounter = 0;
		scanDir(sli.dir, path);
		sli.flags = scanShareFlags & ~SCAN_SHARE_FLAG_REBUILD_BLOOM;
		sli.totalFiles = fileCounter;
		scanAllFlags |= scanShareFlags;
	}

#ifdef DEBUG_SHARE_MANAGER
	LogManager::message("Finished scanning directories", false);
#endif

	{
		bool updateIndex = false;
		WRITE_LOCK(*csShare);
		shareListChanged = false;
		for (auto i = shares.begin(); i != shares.end();)
		{
			if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED)
			{
				SharedDir::deleteTree(i->dir);
				i = shares.erase(i);
				continue;
			}
			for (auto j = newShares.begin(); j != newShares.end(); ++j)
				if (i->realPath.getLowerName() == j->realPath.getLowerName())
				{
					if (i->version == j->version)
					{
						SharedDir::deleteTree(i->dir);
						i->dir = j->dir;
						if (j->flags) i->version++;
						i->totalFiles = j->totalFiles;
#ifdef DEBUG_SHARE_MANAGER
						LogManager::message("Share " + i->realPath.getName() + ": flags=" + Util::toString(j->flags) + ", version=" + Util::toString(i->version));
#endif
						newShares.erase(j);
					}
					else
					{
#ifdef DEBUG_SHARE_MANAGER
						LogManager::message("Share " + i->realPath.getName() + " was changed during update");
#endif
						SharedDir::deleteTree(j->dir);
						newShares.erase(j);
						updateIndex = true;
						shareListChanged = true;
					}
					break;
				}
			++i;
		}
		if (!newShares.empty())
		{
			updateIndex = true;
			scanAllFlags |= SCAN_SHARE_FLAG_REMOVED | SCAN_SHARE_FLAG_REBUILD_BLOOM;
			for (auto i = newShares.begin(); i != newShares.end(); ++i)
				SharedDir::deleteTree(i->dir);
			newShares.clear();
		}
		if (updateIndex)
		{
			LogManager::message("TTH Index will be rebuilt", false);
			tthIndex.clear();
			tthIndexNew.clear();
			for (auto i = shares.cbegin(); i != shares.cend(); ++i)
				updateIndexDirL(i->dir);
		}
		else
		{
			tthIndex = std::move(tthIndexNew);
			tthIndexNew.clear();
			csHashBloom.lock();
			hashBloom.clear();
			csHashBloom.unlock();
		}
		if (scanAllFlags & SCAN_SHARE_FLAG_REBUILD_BLOOM)
			updateBloomL();
		else
			bloom = std::move(bloomNew);
		updateSharedSizeL();
#ifdef DEBUG_SHARE_MANAGER
		for (const auto& i : shareGroups)
		{
			const ShareGroup& sg = i.second;
			LogManager::message("Share group " + sg.id.toBase32() + ": size=" + Util::toString(sg.totalSize) + ", files=" + Util::toString(sg.totalFiles), false);
		}
		LogManager::message("Total: size=" + Util::toString(totalSize) + ", files=" + Util::toString(totalFiles), false);
#endif
	}

	{
		LOCK(csSearchCache);
		searchCache.clear();
	}

	if (!filesToHash.empty())
	{
		HashManager* hm = HashManager::getInstance();
		for (auto i = filesToHash.begin(); i != filesToHash.end(); ++i)
		{
			maxSharedFileID = ++nextFileID;
			hm->hashFile(maxSharedFileID, i->file, i->path, i->file->size);
		}
		filesToHash.clear();
		filesToHash.shrink_to_fit();
		doingHashFiles.store(true);
	}
	else
	{
		tickLastRefresh = GET_TICK();
		if (autoRefreshTime)
			tickRefresh = tickLastRefresh + autoRefreshTime;
		else
			tickRefresh = std::numeric_limits<uint64_t>::max();
		tickUpdateList.store(0);
	}

	if (scanAllFlags & (SCAN_SHARE_FLAG_ADDED | SCAN_SHARE_FLAG_REMOVED))
		ClientManager::infoUpdated(true);
	finishedScanDirs.store(true);

	uint64_t elapsed = (GET_TICK() - startTick + 999) / 1000;
	LogManager::message(STRING_F(FILE_LIST_REFRESH_SCANNED, elapsed));
}

bool ShareManager::refreshShare()
{
	bool prevStatus = false;
	if (!doingScanDirs.compare_exchange_strong(prevStatus, true))
		return false;
	finishedScanDirs = false;
	timeLastRefresh = GET_TIME();
	start(0, "ShareManager");
	return true;
}

bool ShareManager::refreshShareIfChanged()
{
	{
		READ_LOCK(*csShare);
		if (!shareListChanged)
		{
			if (fileListChanged)
				tickUpdateList.store(0);
			return false;
		}
	}
	return refreshShare();
}

void ShareManager::generateFileList() noexcept
{
	tickUpdateList.store(0);
}

void ShareManager::updateIndexDirL(const SharedDir* dir) noexcept
{
	ShareManager::TTHMapItem tthItem;
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const SharedFilePtr& file = i->second;
		if (file->flags & BaseDirItem::FLAG_HASH_FILE) continue;
		tthItem.dir = dir;
		tthItem.file = file;
		tthIndex.insert(make_pair(file->getTTH(), tthItem));
	}
	for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
		updateIndexDirL(i->second);
}

void ShareManager::updateBloomDirL(const SharedDir* dir) noexcept
{
	bloom.add(dir->getLowerName());
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
		bloom.add(i->second->getLowerName());
	for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
		updateBloomDirL(i->second);
}

void ShareManager::updateBloomL() noexcept
{
	LogManager::message("Bloom will be rebuilt", false);
	bloom.clear();
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		if (!(i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED))
			updateBloomDirL(i->dir);
}

void ShareManager::updateSharedSizeL() noexcept
{
	int64_t totalFiles = 0;
	int64_t totalSize = 0;
	bool calcTotal = true;
	for (auto& j : shareGroups)
	{
		ShareGroup& sg = j.second;
		sg.totalFiles = 0;
		sg.totalSize = 0;
		for (const auto& sli : shares)
		{
			if (sli.dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
			if (calcTotal)
			{
				totalFiles += sli.totalFiles;
				totalSize += sli.dir->totalSize;
			}
			for (const auto& share : sg.shares)
				if (share.getLowerName() == sli.realPath.getLowerName())
				{
					sg.totalFiles += sli.totalFiles;
					sg.totalSize += sli.dir->totalSize;
					break;
				}
		}
		calcTotal = false;
	}
	this->totalSize.store(totalSize);
	this->totalFiles.store(totalFiles);
}

void ShareManager::initDefaultShareGroupL() noexcept
{
	CID id;
	auto i = shareGroups.find(id);
	if (i != shareGroups.end())
	{
		ShareGroup& sg = i->second;
		sg.attrUncomp = fileAttr[FILE_ATTR_FILES_XML];
		sg.attrComp = fileAttr[FILE_ATTR_FILES_BZ_XML];
		return;
	}
	ShareGroup sg;
	for (const auto& sli : shares)
		sg.shares.emplace_back(sli.realPath);
	sg.attrUncomp = fileAttr[FILE_ATTR_FILES_XML];
	sg.attrComp = fileAttr[FILE_ATTR_FILES_BZ_XML];
	shareGroups.insert(make_pair(id, sg));
}

bool ShareManager::updateShareGroupHashL(ShareGroup& sg)
{
	TigerHash tiger;
	for (BaseDirItem& item : sg.shares)
	{
		for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		{
			if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
			if (i->realPath.getLowerName() == item.getLowerName())
			{
				const string& path = item.getLowerName();
				tiger.update(path.data(), path.length());
				tiger.update(&i->version, sizeof(i->version));
				const string& virtualPath = i->dir->getLowerName();
				tiger.update(virtualPath.data(), virtualPath.length());
				break;
			}
		}
	}
	const uint8_t* hash = tiger.finalize();
	if (!memcmp(hash, sg.hash.data, TigerHash::BYTES)) return false;
	memcpy(sg.hash.data, hash, TigerHash::BYTES);
	return true;
}

size_t ShareManager::getSharedTTHCount() const noexcept
{
	READ_LOCK(*csShare);
	return tthIndex.size();
}

void ShareManager::getDirectories(vector<SharedDirInfo>& res) const noexcept
{
	res.clear();
	READ_LOCK(*csShare);
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		const SharedDir* dir = i->dir;
		if (!(dir->flags & BaseDirItem::FLAG_SHARE_REMOVED))
			res.emplace_back(dir->getName(), i->realPath.getName(),
				(dir->flags & BaseDirItem::FLAG_SIZE_UNKNOWN) ? -1 : dir->totalSize);
	}
	for (auto i = notShared.cbegin(); i != notShared.cend(); ++i)
		res.emplace_back(*i);
}

void ShareManager::getShareGroups(vector<ShareGroupInfo>& res) const noexcept
{
	res.clear();
	READ_LOCK(*csShare);
	for (auto i = shareGroups.cbegin(); i != shareGroups.cend(); ++i)
	{
		const ShareGroup& sg = i->second;
		if (!sg.id.isZero())
			res.emplace_back(ShareGroupInfo{sg.id, sg.name});
	}
}

bool ShareManager::getShareGroupDirectories(const CID& id, boost::unordered_set<string>& dirs) const noexcept
{
	dirs.clear();
	READ_LOCK(*csShare);
	auto i = shareGroups.find(id);
	if (i == shareGroups.cend()) return false;
	const auto& shares = i->second.shares;
	for (const auto& share : shares)
		dirs.insert(share.getLowerName());
	return true;
}

bool ShareManager::getShareGroupDirectories(const CID& id, list<string>& dirs) const noexcept
{
	dirs.clear();
	READ_LOCK(*csShare);
	auto i = shareGroups.find(id);
	if (i == shareGroups.cend()) return false;
	const auto& shares = i->second.shares;
	for (const auto& share : shares)
		dirs.push_back(share.getName());
	return true;
}

string ShareManager::getBZXmlFile(const CID& id, int64_t& xmlSize) const noexcept
{
	string path = Util::getConfigPath();
	if (!id.isZero())
		path += "ShareGroups" PATH_SEPARATOR_STR + id.toBase32() + PATH_SEPARATOR;
	READ_LOCK(*csShare);
	auto i = shareGroups.find(id);
	if (i == shareGroups.end())
	{
		xmlSize = 0;
		return Util::emptyString;
	}
	const ShareGroup& sg = i->second;
	xmlSize = sg.attrUncomp.size;
	if (sg.tempXmlFile.empty())
		path += fileBZXml;
	else
		path += sg.tempXmlFile;
	return path;
}

bool ShareManager::renameXmlFiles() noexcept
{
	bool result = true;
	WRITE_LOCK(*csShare);
	for (auto& i : shareGroups)
	{
		ShareGroup& sg = i.second;
		if (sg.tempXmlFile.empty()) continue;
		string tempXmlPath = Util::getConfigPath();
		if (!sg.id.isZero())
			tempXmlPath += "ShareGroups" PATH_SEPARATOR_STR + sg.id.toBase32() + PATH_SEPARATOR;
		string origXmlPath = tempXmlPath + fileBZXml;
		tempXmlPath += sg.tempXmlFile;
		if (File::renameFile(tempXmlPath, origXmlPath))
			sg.tempXmlFile.clear();
		else
			result = false;
	}
	return result;
}

bool ShareManager::changed() const noexcept
{
	READ_LOCK(*csShare);
	return shareListChanged;
}

bool ShareManager::isInSkipList(const string& lowerName) const
{
	LOCK(csSkipList);
	return hasSkipList && std::regex_match(lowerName, reSkipList);
}

void ShareManager::rebuildSkipList()
{
	std::regex re;
	bool result = Wildcards::regexFromPatternList(re, SETTING(SKIPLIST_SHARE), true);
	LOCK(csSkipList);
	reSkipList = std::move(re);
	hasSkipList = result;
}

void ShareManager::on(FileHashed, int64_t fileID, const SharedFilePtr& file, const string& fileName, const TTHValue& root, int64_t size) noexcept
{
	if (!file) return;
	string pathLower;
	Text::toLower(fileName, pathLower);
	
	WRITE_LOCK(*csShare);
	file->tth = root;
	file->flags &= ~BaseDirItem::FLAG_HASH_FILE;

	file->timeShared = Util::getFileTime();

	SharedFilePtr storedFile;
	SharedDir* dir;
	if (findByRealPathL(pathLower, dir, storedFile))
	{
		TTHMapItem tthItem;
		tthItem.file = file;
		tthItem.dir = dir;
		tthIndex.insert(make_pair(root, tthItem));
	}
	if (fileID > maxHashedFileID)
		maxHashedFileID = fileID;
}

void ShareManager::on(HashingError, int64_t fileID, const SharedFilePtr& file, const string& fileName) noexcept
{
	if (!file) return;
	string pathLower;
	Text::toLower(fileName, pathLower);
	
	WRITE_LOCK(*csShare);
	SharedFilePtr storedFile;
	SharedDir* dir;
	if (findByRealPathL(pathLower, dir, storedFile))
		dir->files.erase(storedFile->getLowerName());
	if (fileID > maxHashedFileID)
		maxHashedFileID = fileID;
}

void ShareManager::on(HashingAborted) noexcept
{
	tickLastRefresh = GET_TICK();
	if (autoRefreshTime)
		tickRefresh = tickLastRefresh + autoRefreshTime;
	else
		tickRefresh = std::numeric_limits<uint64_t>::max();
	doingHashFiles.store(false);
	tickUpdateList.store(0);
	generateFileList(0);
}

void ShareManager::on(Second, uint64_t tick) noexcept
{
	if (doingScanDirs)
	{
		if (!finishedScanDirs) return;
		join();
		doingScanDirs.store(false);
	}
	if (doingHashFiles && maxHashedFileID >= maxSharedFileID)
	{
		tickLastRefresh = tick;
		if (autoRefreshTime)
			tickRefresh = tickLastRefresh + autoRefreshTime;
		else
			tickRefresh = std::numeric_limits<uint64_t>::max();
		tickUpdateList.store(0);
		doingHashFiles.store(false);
	}
	if (tick > tickRestoreFileList)
	{
		if (renameXmlFiles())
			tickRestoreFileList.store(std::numeric_limits<uint64_t>::max());
		else
			tickRestoreFileList.store(tick + 60000);
	}
	generateFileList(tick);
	if (!doingHashFiles && tick > tickRefresh)
		refreshShare();
}

void ShareManager::on(Repaint) noexcept
{
	unsigned newAutoRefreshTime = SETTING(AUTO_REFRESH_TIME) * 60000;
	if (newAutoRefreshTime == autoRefreshTime) return;
	autoRefreshTime = newAutoRefreshTime;
	if (autoRefreshTime)
		tickRefresh = tickLastRefresh + autoRefreshTime;
	else
		tickRefresh = std::numeric_limits<uint64_t>::max();
}

void ShareManager::shutdown()
{
	HashManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);
	SettingsManager::getInstance()->removeListener(this);
	if (doingScanDirs)
	{
		stopScanning.store(true);
		join();
		doingHashFiles.store(false);
		doingScanDirs.store(false);
	}
}

bool ShareManager::isRefreshing() const noexcept
{
	return doingScanDirs || doingHashFiles || doingCreateFileList;
}

int ShareManager::getState() const noexcept
{
	if (doingScanDirs) return STATE_SCANNING_DIRS;
	if (doingHashFiles) return STATE_HASHING_FILES;
	if (doingCreateFileList) return STATE_CREATING_FILELIST;
	return STATE_IDLE;
}

int64_t ShareManager::getShareListVersion() const noexcept
{
	READ_LOCK(*csShare);
	return shareListVersion;
}

void ShareManager::getScanProgress(int64_t result[]) const noexcept
{
	result[0] = scanProgress[0];
	result[1] = scanProgress[1];
}

bool ShareManager::getShareGroupInfo(const CID& id, int64_t& size, int64_t& files) const noexcept
{
	READ_LOCK(*csShare);
	auto i = shareGroups.find(id);
	if (i != shareGroups.cend())
	{
		size = i->second.totalSize;
		files = i->second.totalFiles;
		return true;
	}
	size = files = 0;
	return false;
}

bool ShareManager::getShareGroupName(const CID& id, string& name) const noexcept
{
	READ_LOCK(*csShare);
	auto i = shareGroups.find(id);
	if (i != shareGroups.cend())
	{
		name = i->second.name;
		return true;
	}
	return false;
}

static const string attrFileName[] =
{
	"files.xml",
	"files.xml.bz2",
	"EmptyFiles.xml",
	"EmptyFiles.xml.bz2"
};

bool ShareManager::readFileAttr(const string& path, FileAttr attr[MAX_FILE_ATTR], CID& cid) noexcept
{
	cid.init();
	for (int i = 0; i < MAX_FILE_ATTR; ++i) attr[i].size = -1;
	bool result = false;
	try
	{
		const string fileData = File(path, File::READ, File::OPEN).read();
		SimpleXML xml;
		xml.fromXML(fileData);
		if (xml.findChild(tagFileListing))
		{
			cid.fromBase32(xml.getChildAttrib("CID"));
			xml.stepIn();
			while (xml.findChild(tagFile))
			{
				const string& name = xml.getChildAttrib(attrName);
				for (int i = 0; i < MAX_FILE_ATTR; ++i)
					if (!stricmp(name, attrFileName[i]))
					{
						const string& size = xml.getChildAttrib(attrSize);
						const string& tth = xml.getChildAttrib(attrTTH);
						if (!size.empty() && tth.length() == 39)
						{
							attr[i].size = Util::toInt64(size);
							Encoder::fromBase32(tth.c_str(), attr[i].root.data, TTHValue::BYTES);
							result = true;
						}
						break;
					}
			}
		}
	}
	catch (Exception&)
	{
		result = false;
	}
	return result;
}

bool ShareManager::writeFileAttr(const string& path, const FileAttr attr[MAX_FILE_ATTR]) const noexcept
{
	bool result = true;
	try
	{
		File xml(path, File::WRITE, File::TRUNCATE | File::CREATE);
		string data = SimpleXML::utf8Header;
		data.append(LITERAL("<FileListing CID=\""));
		data.append(ClientManager::getMyCID().toBase32());
		data.append(LITERAL("\">\n"));
		for (int i = 0; i < MAX_FILE_ATTR; ++i)
		{
			if (attr[i].size < 0) continue;
			data.append(LITERAL("\t<File Name=\""));
			data.append(attrFileName[i]);
			data.append(LITERAL("\" Size=\""));
			data.append(Util::toString(attr[i].size));
			data.append(LITERAL("\" TTH=\""));
			data.append(attr[i].root.toBase32());
			data.append(LITERAL("\"/>\n"));
		}
		data.append(LITERAL("</FileListing>"));
		xml.write(data);
	}
	catch (FileException&)
	{
		result = false;
	}
	return result;
}

bool ShareManager::writeEmptyFileList(const string& path) noexcept
{
	bool result = true;
	try
	{
		File outFileXml(path, File::WRITE, File::TRUNCATE | File::CREATE);
		FilteredOutputStream<FileListFilter, false> newXmlFile(&outFileXml);
		newXmlFile.write(SimpleXML::utf8Header);
		newXmlFile.write("<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"/\" Generator=\"DC++ " DCVERSIONSTRING "\">\r\n");
		newXmlFile.write("</FileListing>");
		newXmlFile.flushBuffers(true);

		newXmlFile.getFilter().treeOriginal.finalize(fileAttr[FILE_ATTR_EMPTY_FILES_XML].root);
		newXmlFile.getFilter().treeCompressed.finalize(fileAttr[FILE_ATTR_EMPTY_FILES_BZ_XML].root);
		fileAttr[FILE_ATTR_EMPTY_FILES_XML].size = newXmlFile.getFilter().sizeOriginal;
		fileAttr[FILE_ATTR_EMPTY_FILES_BZ_XML].size = newXmlFile.getFilter().sizeCompressed;
	}
	catch (const Exception& e)
	{
		LogManager::message("Error creating " + path + ": " + e.getError());
		File::deleteFile(path);
		result = false;
	}
	return result;
}

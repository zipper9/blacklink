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
#include "UserConnection.h"
#include "CFlylinkDBManager.h"
#include "LogManager.h"
#include "DebugManager.h"

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
		uint16_t filesTypesMask;
};

void ShareLoader::startTag(const string& name, StringPairList& attribs, bool simple)
{
	if (manager.stopLoading)
		throw ShareLoaderException("Stopped");
	
	if (inListing)
	{
		if (name == tagFile)
		{
			if (!current) return;

			const string *valFilename = nullptr;
			const string *valTTH = nullptr;
			const string *valSize = nullptr;
			const string *valHit = nullptr;
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
				if (attrib == attrHit) valHit = &value; else
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
				if (val > 0) timeShared = val * 10000000 + 116444736000000000ll;
			}

			unsigned hit = valHit ? Util::toUInt32(*valHit) : 0;
			manager.loadSharedFile(current, *valFilename, size, tth, 0, timeShared, hit);
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

void ShareManager::loadSharedFile(SharedDir* current, const string& filename, int64_t size, const TTHValue& tth, uint64_t timestamp, uint64_t timeShared, unsigned hit) noexcept
{
	SharedFilePtr file = std::make_shared<SharedFile>(filename, tth, size, timestamp, timeShared, getFileTypesFromFileName(filename), hit);
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
			current = it->dir;
	}			
}

void ShareManager::loadShareList(SimpleXML& aXml)
{
	CFlyWriteLock(*csShare);
	
	aXml.resetCurrentChild();
	if (aXml.findChild("Share"))
	{
		aXml.stepIn();
		while (aXml.findChild("Directory"))
		{
			string realPath = aXml.getChildData();
			if (realPath.empty())
			{
				continue;
			}
			// make sure realPath ends with a PATH_SEPARATOR
			Util::appendPathSeparator(realPath);
			
			const string& virtualAttr = aXml.getChildAttrib("Virtual");
			const string virtualName = validateVirtual(virtualAttr.empty() ? Util::getLastDir(realPath) : virtualAttr);
			bool unused;
			if (hasShareL(virtualName, realPath, unused))
			{
				LogManager::message("Duplicate share: real=" + realPath + " virtual=" + virtualAttr + ", skipping...", false);
				continue;
			}
			if (!File::isExist(realPath)) // TODO: is a directory?
				continue;
			ShareListItem shareItem;
			shareItem.realPath.setName(realPath);
			shareItem.dir = new SharedDir(virtualName, nullptr);
			shareItem.version = 0;
			bloom.add(shareItem.dir->getLowerName());
			shares.push_back(shareItem);
		}
		aXml.stepOut();
	}
	aXml.resetCurrentChild();
	if (aXml.findChild("NoShare"))
	{
		aXml.stepIn();
		while (aXml.findChild("Directory"))
		{
			string excludePath = aXml.getChildData();
			Util::appendPathSeparator(excludePath);
			addExcludeFolderL(excludePath);
		}			
		aXml.stepOut();
	}
}

ShareManager::ShareManager() :
	csShare(webrtc::RWLockWrapper::CreateRWLock()),
	totalSize(0),
	totalFiles(0),
	fileCounter(0),
	shareListChanged(false),
	versionCounter(0),
	bloom(1<<20),
	hits(0),
	doingScanDirs(false),
	doingHashFiles(false),
	doingCreateFileList(false),
	stopScanning(false),
	finishedScanDirs(false),
	bloomNew(1<<20),
	hasRemoved(false), hasAdded(false),
	nextFileID(0), maxSharedFileID(0), maxHashedFileID(0),
	optionShareHidden(false), optionShareSystem(false), optionShareVirtual(false),
	optionIncludeHit(false), optionIncludeTimestamp(false),
	stopLoading(false),
	tickUpdateList(std::numeric_limits<uint64_t>::max()),
	tickLastRefresh(0),
	tempFileCount(0),
	hasSkipList(false)
{
	xmlListLen[0] = xmlListLen[1] = 0;	

	const string emptyXmlName = getEmptyBZXmlFile();
	if (!File::isExist(emptyXmlName))
	{
		try
		{
			FilteredOutputStream<BZFilter, true> emptyXmlFile(new File(emptyXmlName, File::WRITE, File::TRUNCATE | File::CREATE));
			emptyXmlFile.write(SimpleXML::utf8Header);
			emptyXmlFile.write("<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"/\" Generator=\"DC++ " DCVERSIONSTRING "\">\r\n");
			emptyXmlFile.write("</FileListing>");
			emptyXmlFile.flushBuffers(true);
		}
		catch (const Exception& e)
		{
			LogManager::message("Error creating " + emptyXmlName + ": " + e.getError());
			File::deleteFile(emptyXmlName);
		}
	}

	autoRefreshTime = SETTING(AUTO_REFRESH_TIME) * 60000;
	if (BOOLSETTING(AUTO_REFRESH_ON_STARTUP))
		tickRefresh = 0;
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
		ATTRIB_MASK_TIME_SHARED = 0x10,
		ATTRIB_MASK_HIT         = 0x20
	};

	BufferedInputStream<false> bs(&file, 256 * 1024);
	uint8_t buf[64 * 1024];
	ShareLoaderMode mode = MODE_NONE;
	SharedDir* current = nullptr;
	string name;
	int64_t fileSize = 0;
	uint64_t timestamp = 0;
	uint64_t timeShared = 0;
	unsigned hit = 0;
	TTHValue tth;
	unsigned attribMask = 0;
	for (;;)
	{
		if (stopLoading)
			throw ShareLoaderException("Stopped");
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
				else if (current->flags & BaseDirItem::FLAG_SHARE_LOST)
				{
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
						(attribMask & ATTRIB_MASK_TIME_SHARED) ? timeShared : 0,
						(attribMask & ATTRIB_MASK_HIT) ? hit : 0);
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
				fileSize = *(const int64_t*) buf;
				attribMask |= ATTRIB_MASK_SIZE;
				break;
			case SHARE_DATA_ATTRIB_TTH:
				if (attribSize != TTHValue::BYTES) { invalidSize = true; break; }
				memcpy(tth.data, buf, TTHValue::BYTES);
				attribMask |= ATTRIB_MASK_TTH;
				break;
			case SHARE_DATA_ATTRIB_TIMESTAMP:
				if (attribSize != 8) { invalidSize = true; break; }
				timestamp = *(const int64_t*) buf;
				attribMask |= ATTRIB_MASK_TIMESTAMP;
				break;
			case SHARE_DATA_ATTRIB_TIME_SHARED:
				if (attribSize != 8) { invalidSize = true; break; }
				timeShared = *(const int64_t*) buf;
				attribMask |= ATTRIB_MASK_TIME_SHARED;
				break;
			case SHARE_DATA_ATTRIB_HIT:
				if (attribSize != 4) { invalidSize = true; break; }
				hit = *(const uint32_t*) buf;
				attribMask |= ATTRIB_MASK_HIT;
				break;
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
		outBuf[ptr++] = (size >> 8) | 0x80;
		outBuf[ptr++] = size & 0xFF;
	}
	memcpy(outBuf + ptr, data, size);
	ptr += size;
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
	    !addAttribValue(tempBuf, ptr, SHARE_DATA_ATTRIB_SIZE, &file->size, sizeof(file->size)) ||
		!addAttribValue(tempBuf, ptr, SHARE_DATA_ATTRIB_TTH, file->tth.data, TTHValue::BYTES) ||
		!addAttribValue(tempBuf, ptr, SHARE_DATA_ATTRIB_TIMESTAMP, &file->timestamp, sizeof(file->timestamp)) ||
		(file->timeShared && !addAttribValue(tempBuf, ptr, SHARE_DATA_ATTRIB_TIME_SHARED, &file->timeShared, sizeof(file->timeShared))) ||
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
		_strnicmp(parent.c_str(), dir.c_str(), parent.length()) == 0;
}

static inline bool isSubDirOrSame(const string& dir, const string& parent)
{
	dcassert(!dir.empty() && dir.back() == PATH_SEPARATOR);
	return parent.length() <= dir.length() &&
		_strnicmp(parent.c_str(), dir.c_str(), parent.length()) == 0;
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
		CFlyWriteLock(*csShare);
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
		ShareListItem shareItem;
		shareItem.realPath.setName(realPath);
		shareItem.dir = new SharedDir(virtualName, nullptr);
		shareItem.dir->flags |= BaseDirItem::FLAG_SIZE_UNKNOWN;
		shareItem.version = ++versionCounter;
		bloom.add(shareItem.dir->getLowerName());
		shares.push_back(shareItem);
		shareListChanged = true;
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
	CFlyWriteLock(*csShare);
	for (auto i = shares.begin(); i != shares.end(); ++i)
		if (!(i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) && i->realPath.getLowerName() == pathLower)
		{
			i->dir->flags |= BaseDirItem::FLAG_SHARE_REMOVED;
			found = true;
			break;
		}

	if (!found) return;
	shareListChanged = true;
	
	// Remove corresponding excludes
	for (auto j = notShared.cbegin(); j != notShared.cend();)
	{
		if (isSubDirOrSame(*j, realPath))
			j = notShared.erase(j);
		else
			++j;
	}
}

void ShareManager::renameDirectory(const string& realPath, const string& virtualName)
{
	// FIXME
	removeDirectory(realPath);
	addDirectory(realPath, virtualName);
}

bool ShareManager::addExcludeFolder(const string& path)
{
	dcassert(!path.empty() && path.back() == PATH_SEPARATOR);

	HashManager::getInstance()->stopHashing(path);
	
	CFlyWriteLock(*csShare);
	if (!addExcludeFolderL(path)) return false;
	shareListChanged = true;
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

bool ShareManager::removeExcludeFolder(const string& path)
{
	CFlyWriteLock(*csShare);
	bool result = false;
	for (auto j = notShared.cbegin(); j != notShared.cend();)
	{
		const string& excludedPath = *j;
		if (excludedPath.length() == path.length() &&
		    _strnicmp(path.c_str(), excludedPath.c_str(), path.length()) == 0)
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
	shareListChanged = true;
	return true;
}

bool ShareManager::isDirectoryShared(const string& path) const noexcept
{
	dcassert(!path.empty() && path.back() == PATH_SEPARATOR);

	CFlyReadLock(*csShare);
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
	
	CFlyWriteLock(*csShare);
	SharedDir* dir;
	string unused;
	if (!findByRealPathL(pathLower, dir, unused))
		throw ShareException(STRING(DIRECTORY_NOT_SHARED), path);

	uint64_t currentTime;
	GetSystemTimeAsFileTime((LPFILETIME) &currentTime);
	SharedFilePtr file = std::make_shared<SharedFile>(fileName, root, size, timestamp, currentTime, typesMask, 0);
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

void ShareManager::saveShareList(SimpleXML& aXml) const
{
	CFlyReadLock(*csShare);
	
	aXml.addTag("Share");
	aXml.stepIn();
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		aXml.addTag("Directory", i->realPath.getName());
		aXml.addChildAttrib("Virtual", i->dir->name);
	}
	aXml.stepOut();
	
	aXml.addTag("NoShare");
	aXml.stepIn();
	for (auto j = notShared.cbegin(); j != notShared.cend(); ++j)
	{
		aXml.addTag("Directory", *j);
	}
	aXml.stepOut();
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

bool ShareManager::parseVirtualPathL(const string& virtualPath, const SharedDir* &dir, string& filename) const noexcept
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

bool ShareManager::parseVirtualPathL(const string& virtualPath, const SharedDir* &dir, SharedFilePtr& file) const noexcept
{
	if (virtualPath.empty() || virtualPath.back() == '/')
		return false;
	string filename;
	if (!parseVirtualPathL(virtualPath, dir, filename))
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
	CFlyReadLock(*csShare);
	return tthIndex.find(tth) != tthIndex.end();
}

bool ShareManager::getFilePath(const TTHValue& tth, string& path) const noexcept
{
	CFlyReadLock(*csShare);
	auto it = tthIndex.find(tth);
	if (it == tthIndex.end())
		return false;
	path = getFilePathL(it->second.dir);
	if (!path.empty()) path += it->second.file->getName();
	return !path.empty();
}

bool ShareManager::getFileInfo(AdcCommand& cmd, const string& filename) const noexcept
{
	if (filename == Transfer::g_user_list_name)
	{
		cmd.addParam("FN", filename);
		cmd.addParam("SI", Util::toString(xmlListLen[0]));
		cmd.addParam("TR", xmlListRoot[0].toBase32());
		return true;
	}
	if (filename == Transfer::g_user_list_name_bz)
	{
		cmd.addParam("FN", filename);
		cmd.addParam("SI", Util::toString(xmlListLen[1]));
		cmd.addParam("TR", xmlListRoot[1].toBase32());
		return true;
	}
	
	if (filename.compare(0, 4, "TTH/", 4) != 0)
		return false;

	TTHValue tth;
	bool error;
	Encoder::fromBase32(filename.c_str() + 4, tth.data, sizeof(tth.data), &error);
	if (error) return false;
		
	CFlyReadLock(*csShare);
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

string ShareManager::getFilePathL(const SharedDir* dir) const noexcept
{
	string result;
	if (!dir) return result;
	while (dir->getParent())
	{
		result.insert(0, dir->getName() + PATH_SEPARATOR);
		dir = dir->getParent();
	}
	for (auto it = shares.cbegin(); it != shares.cend(); it++)
		if (it->dir == dir)
		{
			result.insert(0, it->realPath.getName());
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

	CFlyReadLock(*csShare);
	SharedDir* dir;
	SharedFilePtr file;
	if (!findByRealPathL(pathLower, dir, file)) return false;
	if (outTTH) *outTTH = file->getTTH();
	if (outFilename) *outFilename = file->getName();
	if (outSize) *outSize = file->getSize();
	return true;
}

string ShareManager::getFilePath(const string& virtualFile
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
                            , bool isHidingShare
#endif
                           ) const
{
	if (virtualFile == "MyList.DcLst")
		throw ShareException("NMDC-style lists no longer supported, please upgrade your client", virtualFile);
	if (virtualFile == Transfer::g_user_list_name_bz || virtualFile == Transfer::g_user_list_name)
	{
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		if (isHidingShare)
			return getEmptyBZXmlFile();
#endif
		return getBZXmlFile();
	}
	{
		CFlyReadLock(*csShare);
		const SharedDir* dir;
		SharedFilePtr file;
		if (!parseVirtualPathL(virtualFile, dir, file))
			throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualFile);
		return getFilePathL(dir) + file->getName();
	}
}

string ShareManager::getFilePathByTTH(const TTHValue& tth) const
{
	string path;
	if (!getFilePath(tth, path))
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, "TTH/" + tth.toBase32());
	return path;
}

MemoryInputStream* ShareManager::getTreeFromStore(const TTHValue& tth) const noexcept
{
	ByteVector buf;
	try
	{
		TigerTree tree;
		if (CFlylinkDBManager::getInstance()->getTree(tth, tree))
			tree.getLeafData(buf);			
	}
	catch (const Exception&)
	{
		return nullptr;
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

MemoryInputStream* ShareManager::getTree(const string& virtualFile) const noexcept
{
	TTHValue tth;
	{
		CFlyReadLock(*csShare);
		const SharedDir* dir;
		SharedFilePtr file;
		if (!parseVirtualPathL(virtualFile, dir, file))
			return nullptr;
		tth = file->getTTH();
	}
	return getTreeFromStore(tth);
}

void ShareManager::getHashBloom(ByteVector& v, size_t k, size_t m, size_t h) const noexcept
{
	dcdebug("Creating bloom filter, k=%u, m=%u, h=%u\n", unsigned(k), unsigned(m), unsigned(h));
	HashBloom bloom;
	bloom.reset(k, m, h);
	{
		CFlyReadLock(*csShare);
		for (auto i = tthIndex.cbegin(); i != tthIndex.cend(); ++i)
		{
			bloom.add(i->first);
		}
	}
	bloom.copy_to(v);
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

void ShareManager::writeDataL(const SharedDir* dir, OutputStream& xmlFile, OutputStream* shareDataFile, string& indent, string& tmp, uint8_t tempBuf[], bool fullList) const
{
	if (!indent.empty())
		xmlFile.write(indent);
	xmlFile.write(LITERAL("<Directory Name=\""));
	xmlFile.write(SimpleXML::escapeAtrib(dir->getName(), tmp));
	
#ifdef DEBUG_FILELIST
	xmlFile.write("\" FilesTypes=\"");
	xmlFile.write(Util::toString(dir->filesTypesMask));
	xmlFile.write("\" DirsTypes=\"");
	xmlFile.write(Util::toString(dir->dirsTypesMask));
	xmlFile.write("\" TotalSize=\"");
	xmlFile.write(Util::toString(dir->totalSize));
#endif

	if (fullList)
	{
		xmlFile.write(LITERAL("\">\r\n"));		
		indent += '\t';
		if (shareDataFile)
			writeShareDataDirStart(shareDataFile, dir, tempBuf);

		for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
			writeDataL(i->second, xmlFile, shareDataFile, indent, tmp, tempBuf, fullList);
		
		writeFilesDataL(dir, xmlFile, shareDataFile, indent, tmp, tempBuf);
		
		indent.erase(indent.length() - 1);
		if (!indent.empty())
			xmlFile.write(indent);

		xmlFile.write(LITERAL("</Directory>\r\n"));
		if (shareDataFile)
			writeShareDataDirEnd(shareDataFile);
	}
	else
	{
		if (dir->dirs.empty() && dir->files.empty())
			xmlFile.write(LITERAL("\" />\r\n"));
		else
			xmlFile.write(LITERAL("\" Incomplete=\"1\" />\r\n"));
	}
}

void ShareManager::writeFilesDataL(const SharedDir* dir, OutputStream& xmlFile, OutputStream* shareDataFile, string& indent, string& tmp, uint8_t tempBuf[]) const
{
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const auto& f = i->second;
		if (f->flags & BaseDirItem::FLAG_HASH_FILE)
			continue;
		if (!indent.empty())
			xmlFile.write(indent);
		xmlFile.write(LITERAL("<File Name=\""));
		xmlFile.write(SimpleXML::escapeAtrib(f->getName(), tmp));
		xmlFile.write(LITERAL("\" Size=\""));
		xmlFile.write(Util::toString(f->size));
		xmlFile.write(LITERAL("\" TTH=\""));
		tmp.clear();
		xmlFile.write(f->getTTH().toBase32(tmp));
		if (optionIncludeHit && f->hit)
		{
			xmlFile.write(LITERAL("\" HIT=\""));
			xmlFile.write(Util::toString(f->hit));
		}
		if (optionIncludeTimestamp && f->timeShared)
		{
			xmlFile.write(LITERAL("\" Shared=\""));
			xmlFile.write(Util::toString(f->timeShared));
		}
		//xmlFile.write(LITERAL("\" TS=\""));
		//xmlFile.write(Util::toString(f->getTS()));
		xmlFile.write(LITERAL("\"/>\r\n"));
		if (shareDataFile)
			writeShareDataFile(shareDataFile, f, tempBuf);
	}
}

static void deleteTempFiles(const string& fullPath, const string& skipFile)
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
				File::deleteFile(directory + *i);
	}
}

bool ShareManager::generateFileList(uint64_t tick)
{
	if (tick <= tickUpdateList)
		return false;
	
	bool prevStatus = false;
	if (!doingCreateFileList.compare_exchange_strong(prevStatus, true))
		return false;

	LogManager::message("Generating file list...", false);
	uint8_t tempBuf[TEMP_BUF_SIZE];

	optionIncludeHit = BOOLSETTING(FILELIST_INCLUDE_HIT);
	optionIncludeTimestamp = BOOLSETTING(FILELIST_INCLUDE_TIMESTAMP);
	
	const string xmlListFileName = getDefaultBZXmlFile();
	const string shareDataFileName = Util::getConfigPath() + fileShareData;
	string skipBZXmlFile;
	try
	{
		++tempFileCount;
		string tmp;
		string indent;			
		string newXmlName = Util::getConfigPath() + "files" + Util::toString(tempFileCount) + ".xml.bz2";
		string newShareDataName = Util::getConfigPath() + "Share" + Util::toString(tempFileCount) + ".dat";
			
		{
			File outFileXml(newXmlName, File::WRITE, File::TRUNCATE | File::CREATE);
			FilteredOutputStream<FileListFilter, false> newXmlFile(&outFileXml);
				
			File outFileShareData(newShareDataName, File::WRITE, File::TRUNCATE | File::CREATE);
			BufferedOutputStream<false> newShareDataFile(&outFileShareData, 256 * 1024);

			newXmlFile.write(SimpleXML::utf8Header);
			newXmlFile.write(LITERAL("<FileListing Version=\"1\" CID=\""));
			newXmlFile.write(ClientManager::getMyCID().toBase32());
			newXmlFile.write(LITERAL("\" Base=\"/\" Generator=\"DC++ " DCVERSIONSTRING "\">\r\n"));
			{
				CFlyReadLock(*csShare);
				for (auto i = shares.cbegin(); i != shares.cend(); ++i)
					if (!(i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED))
						writeDataL(i->dir, newXmlFile, &newShareDataFile, indent, tmp, tempBuf, true);
			}
			newXmlFile.write(LITERAL("</FileListing>"));
			newXmlFile.flushBuffers(true);

			newShareDataFile.flushBuffers(true);
				
			newXmlFile.getFilter().treeOriginal.finalize(xmlListRoot[0]);
			newXmlFile.getFilter().treeCompressed.finalize(xmlListRoot[1]);

			xmlListLen[0] = newXmlFile.getFilter().sizeOriginal;
			xmlListLen[1] = newXmlFile.getFilter().sizeCompressed;

#ifdef DEBUG_FILELIST
			LogManager::message("Original size: " + Util::toString(xmlListLen[0]), false);
			LogManager::message("Original TTH: " + xmlListRoot[0].toBase32(), false);
			LogManager::message("Compressed size: " + Util::toString(xmlListLen[1]), false);
			LogManager::message("Compressed TTH: " + xmlListRoot[1].toBase32(), false);
#endif
		}
			
		{
			CFlyLock(csTempBZXmlFile);
			try
			{
				File::renameFile(newXmlName, xmlListFileName);
				tempBZXmlFile.clear();
			}
			catch (const FileException&)
			{
				tempBZXmlFile = skipBZXmlFile = Util::getFileName(newXmlName);
			}
		}
		try
		{
			File::renameFile(newShareDataName, shareDataFileName);
			tempShareDataFile.clear();
		}
		catch (const FileException&)
		{
			tempShareDataFile = Util::getFileName(newShareDataName);
		}
		tickUpdateList = std::numeric_limits<uint64_t>::max();
	}
	catch (const Exception& e)
	{
		LogManager::message("Error creating file list: " + e.getError(), false);
		// Retry later
		tickUpdateList = tick + 60000;
	}		
	
	deleteTempFiles(xmlListFileName, skipBZXmlFile);
	deleteTempFiles(shareDataFileName, tempShareDataFile);
	
	doingCreateFileList.store(false);
	return true;
}

MemoryInputStream* ShareManager::generatePartialList(const string& dir, bool recurse
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
                                                     , bool isHidingShare
#endif
                                                    ) const
{
	if (dir.empty() || dir[0] != '/' || dir.back() != '/')
		return nullptr;

	optionIncludeHit = BOOLSETTING(FILELIST_INCLUDE_HIT);
	optionIncludeTimestamp = BOOLSETTING(FILELIST_INCLUDE_TIMESTAMP);

	string xml = SimpleXML::utf8Header;
	string tmp;
	xml += "<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"" + SimpleXML::escape(dir, tmp, false) + "\" Generator=\"DC++ "  DCVERSIONSTRING  "\">\r\n";
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	if (isHidingShare)
	{
		xml += "</FileListing>";
		return new MemoryInputStream(xml);
	}
#endif
	StringOutputStream sos(xml);
	CFlyReadLock(*csShare);
	
	string indent = "\t";
	if (dir == "/")
	{
		for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		{
			if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
			tmp.clear();
			writeDataL(i->dir, sos, nullptr, indent, tmp, nullptr, recurse);
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
			writeDataL(it->second, sos, nullptr, indent, tmp, nullptr, recurse);
		writeFilesDataL(root, sos, nullptr, indent, tmp, nullptr);
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
	string xmlFile = getDefaultBZXmlFile();
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
	totalFiles.store(fileCounter);
	updateSharedSizeL();
	if (!File::isExist(xmlFile))
	{
		try { File::copyFile(getEmptyBZXmlFile(), xmlFile); }
		catch (FileException&) {}
		tickRefresh = 0;
	}
}

bool ShareManager::searchTTH(const TTHValue& tth, vector<SearchResultCore>& results, const Client* client) noexcept
{
	csShare->AcquireLockShared();
	const auto& i = tthIndex.find(tth);
	if (i == tthIndex.end())
	{
		csShare->ReleaseLockShared();
		return false;
	}
	const SharedDir* dir = i->second.dir;
	const SharedFilePtr& file = i->second.file;
	string name = getNMDCPathL(dir) + file->getName();
	const SearchResultCore sr(SearchResult::TYPE_FILE, file->getSize(), name, file->getTTH());
	csShare->ReleaseLockShared();

	incHits();
	results.push_back(sr);

	if (CMD_DEBUG_ENABLED() && client)
		COMMAND_DEBUG("Search TTH=" + tth.toBase32() + " Found=" + name, DebugTask::HUB_IN, client->getIpPort());
	return true;
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
		if (k->matchLower(dir->getLowerName()))
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
		const SearchResultCore sr(SearchResult::TYPE_DIRECTORY, 0, getNMDCPathL(dir), TTHValue());
		results.push_back(sr);
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
			while (j != cur->cend() && j->matchLower(name)) ++j;
			if (j != cur->cend())
				continue;
			
			const SearchResultCore sr(SearchResult::TYPE_FILE, file->getSize(), getNMDCPathL(dir) + file->getName(), file->getTTH());
			results.push_back(sr);
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

// TODO: Use client from SearchParam
void ShareManager::search(vector<SearchResultCore>& results, const SearchParam& sp, const Client* client) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	if (sp.fileType == FILE_TYPE_TTH)
	{
		if (isTTHBase32(sp.filter))
		{
			const TTHValue tth(sp.filter.c_str() + 4);
			searchTTH(tth, results, client);
		}
		return;
	}

#if 0
	const auto rawQuery = sp.getRawQuery();
	if (isUnknownFile(rawQuery))
	{
		return;
	}
	if (isCacheFile(rawQuery, results))
	{
		return;
	}
#endif
	
	const StringTokenizer<string> t(Text::toLower(sp.filter), '$');
	const StringList& sl = t.getTokens();
	{
		bool bloomMatch;
		{
			CFlyReadLock(*csShare);
			bloomMatch = bloom.match(sl);
		}
		if (!bloomMatch)
		{
#if 0
			addUnknownFile(rawQuery);
#endif
			return;
		}
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
		CFlyReadLock(*csShare);
		for (auto j = shares.cbegin(); j != shares.cend(); ++j)
		{
			if (j->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
			searchL(j->dir, results, ssl, sp);
			if (results.size() >= sp.maxResults) break;
		}
	}
#if 0
	if (results.empty())
		addUnknownFile(rawQuery);
	else
		addCacheFile(rawQuery, results);
#endif 0
}

inline static uint16_t toCode(char a, char b)
{
	return (uint16_t)a | ((uint16_t)b) << 8;
}

AdcSearchParam::AdcSearchParam(const StringList& params) noexcept :
	gt(0), lt(std::numeric_limits<int64_t>::max()), hasRoot(false), isDirectory(false)
{
	for (auto i = params.cbegin(); i != params.cend(); ++i)
	{
		const string& p = *i;
		if (p.length() <= 2)
			continue;
			
		const uint16_t cmd = toCode(p[0], p[1]);
		if (toCode('T', 'R') == cmd)
		{
			hasRoot = true;
			root = TTHValue(p.substr(2));
			return;
		}
		else if (toCode('A', 'N') == cmd)
		{
			include.push_back(StringSearch(p.substr(2)));
		}
		else if (toCode('N', 'O') == cmd)
		{
			exclude.push_back(StringSearch(p.substr(2)));
		}
		else if (toCode('E', 'X') == cmd)
		{
			exts.push_back(p.substr(2));
		}
		else if (toCode('G', 'R') == cmd)
		{
			const auto extGroup = AdcHub::parseSearchExts(Util::toInt(p.substr(2)));
			exts.insert(exts.begin(), extGroup.begin(), extGroup.end());
		}
		else if (toCode('R', 'X') == cmd)
		{
			noExts.push_back(p.substr(2));
		}
		else if (toCode('G', 'E') == cmd)
		{
			gt = Util::toInt64(p.substr(2));
		}
		else if (toCode('L', 'E') == cmd)
		{
			lt = Util::toInt64(p.substr(2));
		}
		else if (toCode('E', 'Q') == cmd)
		{
			lt = gt = Util::toInt64(p.substr(2));
		}
		else if (toCode('T', 'Y') == cmd)
		{
			isDirectory = p[2] == '2';
		}
		else if (toCode('T', 'O') == cmd)
		{
			token = p.substr(2);
		}
	}
}

bool AdcSearchParam::isExcluded(const string& str) const noexcept
{
	for (auto i = exclude.cbegin(); i != exclude.cend(); ++i)
		if (i->match(str)) return true;
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

// ADC search
void ShareManager::searchL(const SharedDir* dir, vector<SearchResultCore>& results, AdcSearchParam& sp, const StringSearch::List* replaceInclude, size_t maxResults) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
		
	const StringSearch::List* cur = replaceInclude ? replaceInclude : &sp.include;
	unique_ptr<StringSearch::List> newStr;
	
	// Find any matches in the directory name
	for (auto k = cur->cbegin(); k != cur->cend(); ++k)
	{
		if (k->matchLower(dir->getLowerName()) && !sp.isExcluded(dir->getLowerName()))
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
		const SearchResultCore sr(SearchResult::TYPE_DIRECTORY, dir->totalSize, getNMDCPathL(dir), TTHValue());
		results.push_back(sr);
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
			while (j != cur->cend() && j->matchLower(name)) ++j;
			if (j != cur->cend())
				continue;

			const SearchResultCore sr(SearchResult::TYPE_FILE, file->getSize(), getNMDCPathL(dir) + file->getName(), file->getTTH());
			results.push_back(sr);
			incHits();
			if (results.size() >= maxResults)
				return;
		}
	}	
	for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
	{
		searchL(i->second, results, sp, newStr.get(), maxResults);
		if (results.size() >= maxResults) break;
	}
}

// ADC search
void ShareManager::search(vector<SearchResultCore>& results, AdcSearchParam& sp, size_t maxResults) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
		
	if (sp.hasRoot)
	{
		searchTTH(sp.root, results, nullptr);
		return;
	}
	
	CFlyReadLock(*csShare);
	for (auto i = sp.include.cbegin(); i != sp.include.cend(); ++i)
		if (!bloom.match(i->getPattern()))
			return;

	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED) continue;
		searchL(i->dir, results, sp, nullptr, maxResults);
		if (results.size() >= maxResults) break;
	}
}

bool ShareManager::isDirectoryExcludedL(const string& path) const noexcept
{
	for (auto j = newNotShared.cbegin(); j != newNotShared.cend(); ++j)
		if (isSubDirOrSame(path, *j)) return true;
	return false;
}

void ShareManager::scanDir(SharedDir* dir, const string& path)
{
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
		if (fileName == Util::m_dot || fileName == Util::m_dot_dot || fileName.empty())
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
				if (!hasRemoved)
					bloomNew.add(dir->getLowerName());
				hasAdded = true;
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
			{
				if (isInSkipList(lowerName))
				{
					// !qb, jc!, ob!, dmf, mta, dmfr, !ut, !bt, bc!, getright, antifrag, pusd, dusd, download, crdownload
					LogManager::message(STRING(USER_DENIED_SHARE_THIS_FILE) + ' ' + fileName
					                    + " (" + STRING(SIZE) + ": " + Util::toString(i->getSize()) + ' '
					                    + STRING(B) + ") (" + STRING(DIRECTORY) + ": \"" + path + "\")");
					continue;
				}
				const string fullPath = path + fileName;
				int64_t size = i->getSize();
				if (i->isLink() && size == 0) // https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/14
				{
					try
					{
						File l_get_size(fullPath, File::READ, File::OPEN | File::SHARED);
						size = l_get_size.getSize();
					}
					catch (FileException&)
					{
#if 0
						const auto l_error_code = GetLastError();
						string l_error = "Error share link file: " + fullPath + " error = " + e.getError();
						LogManager::message(l_error);
						// https://msdn.microsoft.com/en-us/library/windows/desktop/ms681382%28v=vs.85%29.aspx
						if (l_error_code != 1920 && l_error_code != 2 && l_error_code != 3 && l_error_code != 21)
						{
							LogManager::message(l_error);
						}
#endif
						continue;
					}
				}

				fileCounter++;
				auto itFile = dir->files.find(lowerName);		
				const uint64_t timestamp = i->getTimeStamp();
				if (itFile != dir->files.end())
				{
					foundFiles++;
					SharedFilePtr& file = itFile->second;
					filesTypesMask |= file->getFileTypes();
					if (file->size == size && file->timestamp == timestamp)
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
				deltaSize += newFile->getSize();
				if (!hasRemoved)
					bloomNew.add(newFile->getLowerName());				
#ifdef DEBUG_SHARE_MANAGER
				LogManager::message("New file: " + fullPath, false);
#endif
				FileToHash fth;
				fth.file = newFile;
				fth.path = fullPath;
				filesToHash.push_back(fth);
			}
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
				hasRemoved = true;
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
				hasRemoved = true;
			} else ++i;
		}
	}
	if (deltaSize)
		dir->updateSize(deltaSize);
	if (hasRemoved)
		dir->updateTypes(filesTypesMask, dirsTypesMask);
	else
		dir->addTypes(filesTypesMask, dirsTypesMask);
}

void ShareManager::scanDirs()
{
	LogManager::message(STRING(FILE_LIST_REFRESH_INITIATED));

	optionShareHidden = BOOLSETTING(SHARE_HIDDEN);
	optionShareSystem = BOOLSETTING(SHARE_SYSTEM);
	optionShareVirtual = BOOLSETTING(SHARE_VIRTUAL);

	fileCounter = 0;
	ShareList newShares;
	ShareListItem shareItem;
	{
		CFlyReadLock(*csShare);
		hasRemoved = hasAdded = false;
#ifdef DEBUG_SHARE_MANAGER
		uint64_t tsStart = GET_TICK();
#endif
		for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		{
			if (i->dir->flags & BaseDirItem::FLAG_SHARE_REMOVED)
			{
				hasRemoved = true;
				continue;
			}
			shareItem.dir = SharedDir::copyTree(i->dir);
			shareItem.realPath = i->realPath;
			shareItem.version = i->version;
			newShares.push_back(shareItem);
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
	for (auto i = newShares.begin(); i != newShares.end(); ++i)
	{
		if (stopScanning) break;
		const string& path = i->realPath.getName();
		LogManager::message("Scanning share: " + path, false);
		scanDir(i->dir, path);
	}

#ifdef DEBUG_SHARE_MANAGER
	LogManager::message("Finished scanning directories", false);
#endif
	if (!filesToHash.empty()) hasAdded = true;

	{
		bool updateIndex = false;
		CFlyWriteLock(*csShare);
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
			hasRemoved  = true;
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
		}
		if (hasRemoved)
		{
			LogManager::message("Bloom will be rebuilt", false);
			for (auto i = shares.cbegin(); i != shares.cend(); ++i)
				updateBloomDirL(i->dir);
		}
		else
		{
			bloom = std::move(bloomNew);
		}
		updateSharedSizeL();
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
	
	totalFiles.store(fileCounter);
	if (hasAdded || hasRemoved)
		ClientManager::infoUpdated(true);
	finishedScanDirs.store(true);
	LogManager::message(STRING(FILE_LIST_REFRESH_FINISHED));
}

bool ShareManager::refreshShare()
{
	bool prevStatus = false;
	if (!doingScanDirs.compare_exchange_strong(prevStatus, true))
		return false;
	finishedScanDirs = false;
	start();
	return true;
}

bool ShareManager::refreshShareIfChanged()
{
	CFlyReadLock(*csShare);
	if (!shareListChanged)
		return false;
	return refreshShare();
}

void ShareManager::generateFileList()
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
	bloomNew.add(dir->getLowerName());
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
		bloomNew.add(i->second->getLowerName());
	for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
		updateBloomDirL(i->second);
}

void ShareManager::updateSharedSizeL() noexcept
{
	int64_t size = 0;
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		size += i->dir->totalSize;
	totalSize.store(size);
}

size_t ShareManager::getSharedTTHCount() const noexcept
{
	CFlyReadLock(*csShare);
	return tthIndex.size();
}

void ShareManager::getDirectories(vector<SharedDirInfo>& res) const noexcept
{
	res.clear();
	CFlyReadLock(*csShare);	
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

string ShareManager::getBZXmlFile() const noexcept
{
	{
		CFlyLock(csTempBZXmlFile);
		if (!tempBZXmlFile.empty()) return Util::getConfigPath() + tempBZXmlFile;
	}
	return getDefaultBZXmlFile();
}

bool ShareManager::changed() const noexcept
{
	CFlyReadLock(*csShare);
	return shareListChanged;
}

bool ShareManager::isInSkipList(const string& lowerName) const
{
	CFlyFastLock(csSkipList);
	return hasSkipList && std::regex_match(lowerName, reSkipList);
}

void ShareManager::rebuildSkipList()
{
	std::regex re;
	bool result = Wildcards::regexFromPatternList(re, SETTING(SKIPLIST_SHARE), true);
	CFlyFastLock(csSkipList);
	reSkipList = std::move(re);
	hasSkipList = result;
}

void ShareManager::on(TTHDone, int64_t fileID, const SharedFilePtr& file, const string& fileName, const TTHValue& root, int64_t size) noexcept
{
	if (!file) return;
	string pathLower;
	Text::toLower(fileName, pathLower);
	
	CFlyWriteLock(*csShare);
	file->tth = root;
	file->size = size;
	file->flags &= ~BaseDirItem::FLAG_HASH_FILE;
	
	uint64_t currentTime;
	GetSystemTimeAsFileTime((LPFILETIME) &currentTime);
	file->timeShared = currentTime;

	SharedFilePtr storedFile;
	SharedDir* dir;
	if (findByRealPathL(pathLower, dir, storedFile))
	{
		TTHMapItem tthItem;
		tthItem.file = file;
		tthItem.dir = dir;
		tthIndex.insert(make_pair(root, tthItem));
		//maxHashedFileID.store(fileID);
		if (fileID > maxHashedFileID)
			maxHashedFileID = fileID;
	}
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
	if (!tempBZXmlFile.empty())
	{
		try
		{
			File::renameFile(Util::getConfigPath() + tempBZXmlFile, getDefaultBZXmlFile());
			tempBZXmlFile.clear();
		}
		catch (FileException&) {}
	}
	if (!tempShareDataFile.empty())
	{
		try
		{
			File::renameFile(Util::getConfigPath() + tempShareDataFile, Util::getConfigPath() + fileShareData);
			tempShareDataFile.clear();
		}
		catch (FileException&) {}
	}
}

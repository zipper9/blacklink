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
#include "DirectoryListing.h"
#include "QueueManager.h"
#include "SearchManager.h"
#include "DatabaseManager.h"
#include "SimpleStringTokenizer.h"
#include "SimpleXML.h"
#include "FilteredFile.h"
#include "PathUtil.h"
#include "TimeUtil.h"
#include "FormatUtil.h"
#include "BZUtils.h"
#include "HashUtil.h"
#include "SimpleXMLReader.h"
#include "User.h"
#include "ShareManager.h"
#include "ClientManager.h"
#include "SettingsManager.h"
#include "ConfCore.h"

static const int PROGRESS_REPORT_TIME = 2000;

#ifdef _WIN32
static wchar_t *utf8ToWidePtr(const string& str) noexcept
{
	size_t size = 0;
	size_t outSize = str.length() + 1;
	wchar_t *out = new wchar_t[outSize];
	while ((size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), out, (int) outSize)) == 0)
	{
		delete[] out;
		int error = GetLastError();
		if (error == ERROR_INSUFFICIENT_BUFFER)
		{
			outSize <<= 1;
			out = new wchar_t[outSize];
		} else
		{
			dcassert(0);
			return nullptr;
		}
		
	}
	out[size] = 0;
	return out;
}
#endif

static bool checkFormat(const char *s, const char *fmt)
{
	for (;;)
	{
		if (*fmt == '0')
		{
			if (*s < '0' || *s > '9') return false;
		} else
		if (*fmt != *s) return false;
		if (!*s) break;
		s++;
		fmt++;
	}
	return true;
}

DirectoryListing::DirectoryListing(std::atomic_bool& abortFlag, bool createRoot, const DirectoryListing* src) :
	abortFlag(abortFlag),
	includeSelf(false), incomplete(false), aborted(false), scanOptions(0)
{
	root = createRoot ? new Directory(nullptr, Util::emptyString, true) : nullptr;
	tthSet = createRoot ? nullptr : new TTHMap;
	ownList = src ? src->ownList : false;
	hasTimestampsFlag = src ? src->hasTimestampsFlag : false;
}

DirectoryListing::~DirectoryListing()
{
	delete root;
	delete tthSet;
}

static const string extBZ2 = ".bz2";
static const string extXML = ".xml";

UserPtr DirectoryListing::getUserFromFilename(const string& fileName)
{
	// General file list name format: [username].{DATE.}[CID].[xml|xml.bz2]
	
	string name = Util::getFileName(fileName);
	string ext = Util::getFileExt(name);
	Text::asciiMakeLower(ext);
	
	if (ext == ".dcls" || ext == ".dclst")
	{
		auto user = std::make_shared<User>(CID(), name);
		user->setFlag(User::FAKE);
		return user;
	}
	
	// Strip off any extensions
	if (ext == extBZ2)
	{
		name.erase(name.length() - 4);
		ext = Util::getFileExt(name);
		Text::asciiMakeLower(ext);
	}
	
	if (ext == extXML)
	{
		name.erase(name.length() - 4);
	}

	CID cid;
	bool hasCID = false;
	
	// Find CID
	string::size_type i = name.rfind('.');
	if (i != string::npos)
	{
		size_t n = name.length() - (i + 1);		
		if (n == 39)
		{
			Util::fromBase32(name.c_str() + i + 1, cid.writableData(), CID::SIZE, &hasCID);
			hasCID = !hasCID;
			if (hasCID)
			{
				name.erase(i);
				i = name.rfind('.');
				if (i != string::npos && checkFormat(name.c_str() + i + 1, "00000000_0000"))
					name.erase(i);
				if (cid.isZero())
					hasCID = false;
			}
		}
	}	

	if (!hasCID)
	{
		auto user = std::make_shared<User>(CID(), name);
		user->setFlag(User::FAKE);
		return user;
	}

	return ClientManager::createUser(cid, name, Util::emptyString);
}

void DirectoryListing::loadFile(const string& fileName, ProgressNotif *progressNotif, bool ownList)
{
#ifdef _DEBUG
	LogManager::message("DirectoryListing::loadFile = " + fileName);
#endif
	// For now, we detect type by ending...
	if (!ClientManager::isBeforeShutdown())
	{
		::File ff(fileName, ::File::READ, ::File::OPEN);
		if (Util::checkFileExt(fileName, extBZ2) || Util::isDclstFile(fileName))
		{
			FilteredInputStream<UnBZFilter, false> f(&ff);
			loadXML(f, progressNotif, ownList);
		}
		else if (Util::checkFileExt(fileName, extXML))
		{
			loadXML(ff, progressNotif, ownList);
		}
	}
}

class ListLoader : public SimpleXMLReader::CallBack
{
	public:
		ListLoader(DirectoryListing* list, InputStream &is, DirectoryListing::Directory* root,
		           const UserPtr& user, bool ownList, int scanOptions)
			: list(list), current(root), inListing(false), ownList(ownList),
			  emptyFileNameCounter(0), progressNotif(nullptr), stream(is), filesProcessed(0)
		{
			nextProgressReport = GET_TICK() + PROGRESS_REPORT_TIME;
			list->basePath = "/";
			scanFlags = 0;
			/*
			auto ss = SettingsManager::instance.getCoreSettings();
			ss->lockRead();
			useUploadCounter = ss->getBool(Conf::ENABLE_UPLOAD_COUNTER) && ss->getBool(Conf::FILELIST_SHOW_MY_UPLOADS);
			ss->unlockRead();
			*/
			useUploadCounter = (scanOptions & DirectoryListing::SCAN_OPTION_SHOW_MY_UPLOADS) != 0;
			if (scanOptions & DirectoryListing::SCAN_OPTION_SHARED)
				scanFlags |= DatabaseManager::FLAG_SHARED;
			if (scanOptions & DirectoryListing::SCAN_OPTION_DOWNLOADED)
				scanFlags |= DatabaseManager::FLAG_DOWNLOADED;
			if (scanOptions & DirectoryListing::SCAN_OPTION_CANCELED)
				scanFlags |= DatabaseManager::FLAG_DOWNLOAD_CANCELED;
			hashDb = DatabaseManager::getInstance()->getHashDatabaseConnection();
		}
		
		~ListLoader()
		{
			if (hashDb)
				DatabaseManager::getInstance()->putHashDatabaseConnection(hashDb);
		}
		
		void startTag(const string& name, StringPairList& attribs, bool simple);
		void endTag(const string& name, const string& data);

		void setProgressNotif(DirectoryListing::ProgressNotif *notif)
		{
			progressNotif = notif;
		}

		void fileProcessed()
		{
			if (++filesProcessed == 200)
			{
				filesProcessed = 0;
				notifyProgress();
			}
		}
		
	private:
		DirectoryListing* list;
		DirectoryListing::Directory* current;
		HashDatabaseConnection* hashDb;

		bool inListing;
		bool ownList;
		bool useUploadCounter;
		unsigned emptyFileNameCounter;
		unsigned scanFlags;

		uint64_t nextProgressReport;
		DirectoryListing::ProgressNotif *progressNotif;
		InputStream& stream;
		int filesProcessed;

		void notifyProgress();
};

void DirectoryListing::loadXML(const string& xml, DirectoryListing::ProgressNotif *progressNotif, bool ownList)
{
#if 0
	string debugMessage = "\n\n\n xml: [" + xml + "]\n";
	DumpDebugMessage(_T("received-partial-list.log"), debugMessage.c_str(), debugMessage.length(), false);
#endif
	MemoryInputStream mis(xml);
	loadXML(mis, progressNotif, ownList);
}

void DirectoryListing::loadXML(InputStream& is, DirectoryListing::ProgressNotif *progressNotif, bool ownList)
{
	ListLoader ll(this, is, getRoot(), getUser(), ownList, scanOptions);
	ll.setProgressNotif(progressNotif);
	SimpleXMLReader(&ll).parse(is);
	this->ownList = ownList;
}

static const string tagFileListing = "FileListing";
static const string attrBase = "Base";
static const string attrCID = "CID";
static const string attrGenerator = "Generator";
static const string attrIncludeSelf = "IncludeSelf";
static const string attrIncomplete = "Incomplete";

static const string tagDirectory = "Directory";
static const string tagFile = "File";
static const string attrName = "Name";
static const string attrSize = "Size";
static const string attrTTH = "TTH";
static const string attrHit = "HIT";
static const string attrTS = "TS";
static const string attrBR = "BR";
static const string attrWH = "WH";
static const string attrVideo = "MV";
static const string attrAudio = "MA";
static const string attrShared = "Shared";
static const string attrDate = "Date";

static bool isValidName(const string& name)
{
	if (name.empty()) return false;
	if (name.length() <= 2 && name[0] == '.' && (name.length() == 1 || name[1] == '.')) return false;
	if (name.find('/') != string::npos || name.find('\\') != string::npos) return false;
	return true;
}

void ListLoader::startTag(const string& name, StringPairList& attribs, bool simple)
{
	if (ClientManager::isBeforeShutdown())
	{
		throw AbortException("ListLoader::startTag - ClientManager::isBeforeShutdown()");
	}
	if (list->abortFlag.load())
	{
		list->aborted = true;
		throw AbortException("ListLoader::startTag - " + STRING(ABORT_EM));
	}
	
	if (inListing)
	{
		if (name == tagFile)
		{
			const string *valFilename = nullptr;
			const string *valTTH = nullptr;
			const string *valSize = nullptr;
			const string *valHit = nullptr;
			const string *valTS = nullptr;
			const string *valBR = nullptr;
			const string *valWH = nullptr;
			const string *valVideo = nullptr;
			const string *valAudio = nullptr;
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
				if (attrib == attrBR) valBR = &value; else
				if (attrib == attrWH) valWH = &value; else
				if (attrib == attrVideo) valVideo = &value; else
				if (attrib == attrAudio) valAudio = &value; else
				if (attrib == attrShared) valShared = &value; else
				if (attrib == attrDate) valTS = &value;
			}

			if (!valSize) return;
			if (!valTTH || valTTH->length() != 39) return;

			TTHValue tth;
			bool error;
			Util::fromBase32(valTTH->c_str(), tth.data, sizeof(tth.data), &error);
			if (error) return;

			if (!ownList && tth.isZero()) return;

			int64_t size = Util::toInt64(*valSize);
			if (size < 0) return;

			if (!current)
			{
				if (list->tthSet && size)
					list->tthSet->insert(make_pair(tth, size));
				fileProcessed();
				return;
			}

			int64_t shared = 0;
			if (valShared)
			{
				int64_t val = Util::toInt64(*valShared) - 116444736000000000ll;
				if (val > 0)
				{
					shared = val / 10000000;
					list->hasTimestampsFlag = true;
				}
			}
			else if (valTS)
			{
				int64_t val = Util::toInt64(*valTS);
				if (val > 0)
				{
					shared = val;
					list->hasTimestampsFlag = true;
				}
			}
			else if (current->isAnySet(DirectoryListing::FLAG_DIR_TIMESTAMP))
				shared = current->maxTS;

			uint32_t uploadCount = 0;
			if (valHit && !ownList)
				uploadCount = Util::toUInt32(*valHit);

			MediaInfoUtil::Info tempMedia;
			MediaInfoUtil::Info *media = nullptr;
			
			if (valBR || valWH || valAudio || valVideo)
			{
				tempMedia.bitrate = tempMedia.width = tempMedia.height = 0;
				if (valBR)
					tempMedia.bitrate = Util::toUInt32(*valBR);
				if (valWH)
				{
					string::size_type pos = valWH->find('x');
					if (pos != string::npos)
					{
						tempMedia.width = Util::toUInt32(valWH->c_str());
						tempMedia.height = Util::toUInt32(valWH->c_str() + pos + 1);
					}
				}
				if (valAudio)
				{
					tempMedia.audio = *valAudio;
#if 0
					// ???
					string::size_type pos = tempMedia.audio.find('|');
					if (pos != string::npos && pos && pos + 2 < tempMedia.audio.length())
						tempMedia.audio.erase(0, pos + 2);
#endif
				}
				if (valVideo) tempMedia.video = *valVideo;
				media = &tempMedia;
			}

			DirectoryListing::File* f;
			if (valFilename && isValidName(*valFilename))
				f = new DirectoryListing::File(current, *valFilename, size, tth, uploadCount, shared, media);
			else
				f = new DirectoryListing::File(current, *valTTH, size, tth, uploadCount, shared, media);
			current->files.push_back(f);
			if (shared > current->maxTS) current->maxTS = shared;
			if (media && media->bitrate)
			{
				if (media->bitrate < current->minBitrate) current->minBitrate = media->bitrate;
				if (media->bitrate > current->maxBitrate) current->maxBitrate = media->bitrate;
			}
			
			if (size)
			{
				if (!ownList)
				{
					if (QueueManager::fileQueue.isQueued(f->getTTH()))
					{
						f->setFlag(DirectoryListing::FLAG_QUEUED);
						current->setFlag(DirectoryListing::FLAG_HAS_QUEUED);
					}
					string path;
					if ((scanFlags & DatabaseManager::FLAG_SHARED) && ShareManager::getInstance()->getFileInfo(f->getTTH(), path))
					{
						f->setFlag(DirectoryListing::FLAG_SHARED);
						f->setPath(path);
						current->setFlag(DirectoryListing::FLAG_HAS_SHARED);
					}
					else if (scanFlags & (DatabaseManager::FLAG_DOWNLOADED | DatabaseManager::FLAG_DOWNLOAD_CANCELED))
					{
						unsigned flags;
						if (hashDb && !f->getTTH().isZero())
						{
							hashDb->getFileInfo(f->getTTH().data, flags, nullptr, &path, nullptr, nullptr);
							flags &= scanFlags;
						}
						else
							flags = 0;
						if (flags & DatabaseManager::FLAG_SHARED)
						{
							f->setFlag(DirectoryListing::FLAG_SHARED);
							f->setPath(path);
							current->setFlag(DirectoryListing::FLAG_HAS_SHARED);
						}
						else if (flags & DatabaseManager::FLAG_DOWNLOADED)
						{
							f->setFlag(DirectoryListing::FLAG_DOWNLOADED);
							f->setPath(path);
							current->setFlag(DirectoryListing::FLAG_HAS_DOWNLOADED);
						}
						else if (flags & DatabaseManager::FLAG_DOWNLOAD_CANCELED)
						{
							f->setFlag(DirectoryListing::FLAG_CANCELED);
							current->setFlag(DirectoryListing::FLAG_HAS_CANCELED);
						}
						else
							current->setFlag(DirectoryListing::FLAG_HAS_OTHER);
					}
					else
						current->setFlag(DirectoryListing::FLAG_HAS_OTHER);
				}
				else if (useUploadCounter && hashDb && !f->getTTH().isZero())
				{
					unsigned flags;
					hashDb->getFileInfo(f->getTTH().data, flags, nullptr, nullptr, nullptr, &uploadCount);
					f->setUploadCount(uploadCount);
				}
			}

			current->totalSize += size;
			current->totalUploadCount += uploadCount;

			fileProcessed();
		}
		else if (name == tagDirectory)
		{
			if (!list->tthSet)
			{
				const string &fileName = getAttrib(attribs, attrName, 0);
				const bool incomp = getAttrib(attribs, attrIncomplete, 1) == "1";
				const string& valDate = getAttrib(attribs, attrDate, 1);

				DirectoryListing::Directory* d;
				if (isValidName(fileName))
					d = new DirectoryListing::Directory(current, fileName, !incomp);
				else
					d = new DirectoryListing::Directory(current, "invalid_folder_name_" + Util::toString(++emptyFileNameCounter), !incomp);

				if (!valDate.empty())
				{
					int64_t val = Util::toInt64(valDate);
					if (val > 0)
					{
						d->maxTS = val;
						d->setFlag(DirectoryListing::FLAG_DIR_TIMESTAMP);
						list->hasTimestampsFlag = true;
					}
				}
				current->directories.push_back(d);
				current = d;

				if (incomp)
					list->incomplete = true;
			}
			
			if (simple)
			{
				// To handle <Directory Name="..." />
				endTag(name, Util::emptyString);
			}
		}
	}
	else if (name == tagFileListing)
	{
		const string& b = getAttrib(attribs, attrBase, 2);
		if (!b.empty() && b[0] == '/' && b.back() == '/')
			list->basePath = b;
		
		const string& cidStr = getAttrib(attribs, attrCID, 2);
		if (cidStr.size() == 39)
		{
			const CID cid(cidStr);
			if (!cid.isZero())
			{
				if (!list->getUser())
				{
					UserPtr user = ClientManager::createUser(cid, Util::emptyString, Util::emptyString);
					list->setHintedUser(HintedUser(user, Util::emptyString));
				}
			}
		}
		const string& includeSelf = getAttrib(attribs, attrIncludeSelf, 2);
		list->setIncludeSelf(includeSelf == "1");
		
		inListing = true;
		
		if (simple)
		{
			// To handle <Directory Name="..." />
			endTag(name, Util::emptyString);
		}
	}
}

template<typename T>
struct SortFunc
{
	bool operator()(const T *a, const T *b)
	{
#ifdef _WIN32
		const wchar_t *aName = static_cast<wchar_t*>(a->getUserData());
		const wchar_t *bName = static_cast<wchar_t*>(b->getUserData());
		return Util::defaultSort(aName, bName, false) < 0;
#else
		return stricmp(a->getName(), b->getName()) < 0;
#endif
	}
};

template<typename T>
static void sortList(vector<T*> &data)
{
#ifdef _WIN32
	for (auto it = data.begin(); it != data.end(); it++)
	{
		T *item = *it;
		const string &name = item->getName();
		wchar_t *wname = utf8ToWidePtr(name);
		item->setUserData(wname);
	}
#endif
	sort(data.begin(), data.end(), SortFunc<T>());
#ifdef _WIN32
	for (auto it = data.begin(); it != data.end(); it++)
	{
		T *item = *it;
		wchar_t *wname = static_cast<wchar_t*>(item->getUserData());
		delete[] wname;
		item->setUserData(nullptr);
	}
#endif
}

void ListLoader::endTag(const string& name, const string&)
{
	if (!inListing) return;
	notifyProgress();
	if (name == tagDirectory)
	{
		if (current)
		{
			uint16_t addFlags = 0;
			current->updateSubDirs(addFlags);
			current->setFlags((current->getFlags() & ~DirectoryListing::FLAG_DIR_TIMESTAMP) | addFlags);
			sortList(current->files);
			sortList(current->directories);
			current = current->getParent();
		}
	}
	else if (name == tagFileListing)
	{
		if (current)
		{
			uint16_t unused = 0;
			current->updateSubDirs(unused);
			sortList(current->directories);
		}
		inListing = false;
	}
}

void ListLoader::notifyProgress()
{
	if (progressNotif)
	{
		uint64_t tick = GET_TICK();
		if (tick > nextProgressReport)
		{
			int progress;
			int64_t size = stream.getInputSize();
			int64_t pos = stream.getTotalRead();
			if (size < 0 || pos < 0)
				progress = 0;
			else
			if (pos >= size)
				progress = 100;
			else
				progress = static_cast<int>(pos*100/size);
			progressNotif->notify(progress);
			nextProgressReport = tick + PROGRESS_REPORT_TIME;
		}
	}
}

string DirectoryListing::getPath(const Directory* d) const
{
	if (d == root)
		return Util::emptyString;
		
	string dir;
	dir.reserve(128);
	dir.append(d->getName());
	dir.append(1, PATH_SEPARATOR);
	
	const Directory* cur = d->getParent();
	while (cur != root)
	{
		dir.insert(0, cur->getName() + PATH_SEPARATOR);
		cur = cur->getParent();
	}
	return dir;
}

void DirectoryListing::download(Directory* dir, const string& target, QueueItem::Priority prio, bool& getConnFlag)
{
	string dirTarget = target;
	if (dir != getRoot())
	{
		dirTarget += dir->getName();
		dirTarget += PATH_SEPARATOR;
	}
	if (!dir->getComplete())
	{
		// folder is not completed (partial list?), so we need to download it first
		QueueManager::getInstance()->addDirectory(getPath(dir), hintedUser, dirTarget, prio, QueueManager::DIR_FLAG_DOWNLOAD_DIRECTORY);
	}
	else
	{
		// First, recurse over the directories
		const Directory::List& lst = dir->directories;
		for (auto j = lst.cbegin(); j != lst.cend(); ++j)
			download(*j, dirTarget, prio, getConnFlag);

		// Then add the files
		for (File* file : dir->files)
			download(file, dirTarget + file->getName(), false, prio, false, getConnFlag);
	}
}

void DirectoryListing::download(File* file, const string& target, bool view, QueueItem::Priority prio, bool isDclst, bool& getConnFlag) noexcept
{
	QueueItem::MaskType flags = 0;
	QueueItem::MaskType extraFlags = 0;
	if (view)
	{
		extraFlags |= QueueItem::XFLAG_CLIENT_VIEW;
		if (isDclst)
			flags |= QueueItem::FLAG_DCLST_LIST;
		else
			extraFlags |= QueueItem::XFLAG_TEXT_VIEW;
	}
	try
	{
		QueueManager::QueueItemParams params;
		params.size = file->getSize();
		params.root = &file->getTTH();
		params.priority = prio;
		QueueManager::getInstance()->add(target, params, getUser(), flags, extraFlags, getConnFlag);
		file->setFlag(FLAG_QUEUED);
		Directory* dir = file->getParent();
		while (dir->getParent())
		{
			if (dir->isAnySet(FLAG_HAS_QUEUED)) break;
			dir->setFlag(FLAG_HAS_QUEUED);
			dir = dir->getParent();
		}
	}
	catch (const Exception& e)
	{
		LogManager::message(e.getError());
	}
}

DirectoryListing::Directory::~Directory()
{
	for_each(directories.begin(), directories.end(), [](auto p) { delete p; });
	for_each(files.begin(), files.end(), [](auto p) { delete p; });
}

void DirectoryListing::Directory::filterList(const DirectoryListing::TTHMap& l)
{
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
		(*i)->filterList(l);

	size_t countRemoved = 0;
	directories.erase(std::remove_if(directories.begin(), directories.end(),
		[&countRemoved](const DirectoryListing::Directory *d) -> bool
		{
			if (d->files.size() + d->directories.size() == 0)
			{
				countRemoved++;
				delete d;
				return true;
			}
			return false;
		}), directories.end());
	if (countRemoved)
	{
		Directory* d = this;
		while (d)
		{
			d->totalDirCount -= countRemoved;
			d = d->getParent();
		}
	}

	countRemoved = 0;
	int64_t sizeRemoved = 0;
	files.erase(std::remove_if(files.begin(), files.end(),
		[&l, &countRemoved, &sizeRemoved](const DirectoryListing::File *f) -> bool
		{
			if (l.find(f->getTTH()) != l.end())
			{
				sizeRemoved += f->getSize();
				countRemoved++;
				delete f;
				return true;
			}
			return false;
		}), files.end());
	if (countRemoved)
	{
		Directory* d = this;
		while (d)
		{
			d->totalFileCount -= countRemoved;
			d->totalSize -= sizeRemoved;
			d = d->getParent();
		}
	}
}

void DirectoryListing::Directory::getHashList(DirectoryListing::TTHMap& l) const
{
	for (auto i = files.cbegin(); i != files.cend(); ++i)
	{
		const File* f = *i;
		l.insert(make_pair(f->getTTH(), f->getSize()));
	}

	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		const Directory* d = *i;
		if (!d->getAdls())
			d->getHashList(l);
	}
}

void DirectoryListing::Directory::findDuplicates(DirectoryListing::TTHToFileMap& m, int64_t minSize) const
{
	for (auto i = files.cbegin(); i != files.cend(); ++i)
	{
		File* f = *i;
		if (f->getSize() >= minSize)
			m[f->getTTH()].push_back(f);
	}
	
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		const Directory* d = *i;
		if (!d->getAdls())
			d->findDuplicates(m, minSize);
	}
}

void DirectoryListing::Directory::matchTTHSet(const DirectoryListing::TTHMap& l)
{
	for (auto i = files.cbegin(); i != files.cend(); ++i)
	{
		File* f = *i;
		if (l.contains(f->getTTH()))
			DirectoryListing::markAsFound(f);
	}
	
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		Directory* d = *i;
		if (!d->getAdls())
			d->matchTTHSet(l);
	}
}

void DirectoryListing::buildTTHSet()
{
	dcassert(root);
	if (!tthSet)
		tthSet = new TTHMap;
	else
		tthSet->clear();
	root->getHashList(*tthSet);
}

void DirectoryListing::clearTTHSet()
{
	delete tthSet;
	tthSet = nullptr;
}

void DirectoryListing::findDuplicates(DirectoryListing::TTHToFileMap& m, int64_t minSize)
{
	dcassert(root);
	root->clearMatches();
	TTHToFileMap tmp;
	root->findDuplicates(tmp, minSize);
	m.clear();
	for (auto& i : tmp)
	{
		list<File*> files = std::move(i.second);
		if (files.size() > 1)
		{
			for (File* file : files)
				markAsFound(file);
			m.emplace(i.first, files);
		}
	}
}

void DirectoryListing::matchTTHSet(const TTHMap& l)
{
	dcassert(root);
	root->clearMatches();
	root->matchTTHSet(l);
}

void DirectoryListing::Directory::clearMatches()
{
	if (isAnySet(FLAG_HAS_FOUND))
	{
		for (auto i = files.begin(); i != files.end(); ++i)
			(*i)->unsetFlag(FLAG_FOUND);
		for (auto i = directories.begin(); i != directories.end(); ++i)
			(*i)->clearMatches();
	}
	unsetFlag(FLAG_FOUND | FLAG_HAS_FOUND);
}

const DirectoryListing::File* DirectoryListing::Directory::findFileByHash(const TTHValue& tth) const
{
	for (auto i = files.begin(); i != files.end(); ++i)
	{
		const File* f = *i;
		if (f->getTTH() == tth) return f;
	}
	for (auto i = directories.begin(); i != directories.end(); ++i)
	{
		const Directory* d = *i;
		if (!d->getAdls())
		{
			const File* f = d->findFileByHash(tth);
			if (f) return f;
		}
	}
	return nullptr;
}

void DirectoryListing::Directory::updateSubDirs(MaskType& updatedFlags)
{
	MaskType flags = 0;
	for (auto i = directories.cbegin(); i != directories.cend(); i++)
	{
		const DirectoryListing::Directory *dir = *i;
		if (dir->getAdls()) continue;
		flags |= dir->getFlags() & DirectoryListing::DIR_STATUS_FLAGS;
		totalFileCount += dir->totalFileCount;
		totalDirCount += dir->totalDirCount;
		totalSize += dir->totalSize;
		totalUploadCount += dir->totalUploadCount;
		if (dir->maxTS > maxTS) maxTS = dir->maxTS;
		if (dir->minBitrate < minBitrate) minBitrate = dir->minBitrate;
		if (dir->maxBitrate > maxBitrate) maxBitrate = dir->maxBitrate;
		totalDirCount++;
	}
	totalFileCount += files.size();
	updatedFlags |= flags;
}

void DirectoryListing::Directory::updateFiles(MaskType& updatedFlags)
{
	MaskType flags = 0;
	for (auto i = files.cbegin(); i != files.cend(); i++)
	{
		const DirectoryListing::File *file = *i;
		if (file->isSet(DirectoryListing::FLAG_QUEUED))
			flags |= DirectoryListing::FLAG_HAS_QUEUED;
		if (file->isSet(DirectoryListing::FLAG_SHARED))
			flags |= DirectoryListing::FLAG_HAS_SHARED;
		else
		if (file->isSet(DirectoryListing::FLAG_DOWNLOADED))
			flags |= DirectoryListing::FLAG_HAS_DOWNLOADED;
		else
		if (file->isSet(DirectoryListing::FLAG_CANCELED))
			flags |= DirectoryListing::FLAG_HAS_CANCELED;
		else
			flags |= DirectoryListing::FLAG_HAS_OTHER;
		totalSize += file->getSize();
		totalUploadCount += file->getUploadCount();
		auto shared = file->getTS();
		if (shared > maxTS) maxTS = shared;
		const MediaInfoUtil::Info *media = file->getMedia();
		if (media && media->bitrate)
		{
			if (media->bitrate < minBitrate) minBitrate = media->bitrate;
			if (media->bitrate > maxBitrate) maxBitrate = media->bitrate;
		}
	}
	updatedFlags |= flags;
}

bool DirectoryListing::Directory::updateFlags()
{
	MaskType flags = getFlags() & (FLAG_FOUND | FLAG_HAS_FOUND);
	for (auto i = files.cbegin(); i != files.cend(); i++)
	{
		const DirectoryListing::File *file = *i;
		if (file->isSet(DirectoryListing::FLAG_QUEUED))
			flags |= DirectoryListing::FLAG_HAS_QUEUED;
		if (file->isSet(DirectoryListing::FLAG_SHARED))
			flags |= DirectoryListing::FLAG_HAS_SHARED;
		else
		if (file->isSet(DirectoryListing::FLAG_DOWNLOADED))
			flags |= DirectoryListing::FLAG_HAS_DOWNLOADED;
		else
		if (file->isSet(DirectoryListing::FLAG_CANCELED))
			flags |= DirectoryListing::FLAG_HAS_CANCELED;
		else
			flags |= DirectoryListing::FLAG_HAS_OTHER;
	}
	for (auto i = directories.cbegin(); i != directories.cend(); i++)
	{
		const DirectoryListing::Directory *dir = *i;
		flags |= dir->getFlags() & DirectoryListing::DIR_STATUS_FLAGS;
	}
	if (getFlags() == flags) return false;
	setFlags(flags);
	return true;
}

void DirectoryListing::Directory::updateInfo(DirectoryListing::Directory* dir)
{
	for (;;)
	{
		dir = dir->getParent();
		if (!dir) break;
		
		auto prevTotalFileCount = dir->totalFileCount;
		auto prevTotalDirCount = dir->totalDirCount;
		auto prevTotalSize = dir->totalSize;
		auto prevMaxTS = dir->maxTS;
		auto prevTotalUploadCount = dir->totalUploadCount;
		auto prevMinBitrate = dir->minBitrate;
		auto prevMaxBitrate = dir->maxBitrate;
		
		dir->totalFileCount = 0;
		dir->totalDirCount = 0;
		dir->totalSize = 0;
		dir->maxTS = 0;
		dir->totalUploadCount = 0;
		dir->minBitrate = 0xFFFF;
		dir->maxBitrate = 0;
		
		MaskType flags = 0;
		dir->updateFiles(flags);
		dir->updateSubDirs(flags);

		bool update =
			dir->totalFileCount != prevTotalFileCount ||
			dir->totalDirCount != prevTotalDirCount ||
			dir->totalSize != prevTotalSize ||
			dir->maxTS != prevMaxTS ||
			dir->totalUploadCount != prevTotalUploadCount ||
			dir->maxBitrate != prevMaxBitrate ||
			dir->minBitrate != prevMinBitrate ||
			dir->getFlags() != flags;
		if (dir->getParent()) dir->setFlags(flags);
		if (!update) break;
	}
}

void DirectoryListing::Directory::updateFlags(DirectoryListing::Directory* dir)
{
	while (true)
	{
		DirectoryListing::Directory* p = dir->getParent();
		if (!p) break;
		if (!dir->updateFlags()) break;
		dir = p;
	}
}

DirectoryListing::Directory* DirectoryListing::findDirPath(const string& path) const
{
	if (!root) return nullptr;
	SimpleStringTokenizer<char> sl(path, '/');
	SimpleStringTokenizer<char> slBase(basePath, '/');
	bool matchBase = basePath.length() > 1;
	string token, basePathToken;
	const Directory *dir = root;
	while (sl.getNextNonEmptyToken(token))
	{
		if (matchBase)
		{
			if (slBase.getNextNonEmptyToken(basePathToken))
			{
				if (basePathToken != token) return nullptr;
				continue;
			}
			matchBase = false;
		}
		const Directory *nextDir = nullptr;
		for (auto j = dir->directories.cbegin(); j != dir->directories.cend(); ++j)
		{
			if ((*j)->getName() == token)
			{
				nextDir = *j;
				break;
			}
		}
		if (!nextDir) return nullptr;
		dir = nextDir;
	}
	return const_cast<Directory*>(dir);
}

const DirectoryListing::Directory* DirectoryListing::findDirPathNoCase(const string& path) const
{
	if (!root) return nullptr;
	SimpleStringTokenizer<char> sl(path, '/');
	SimpleStringTokenizer<char> slBase(basePath, '/');
	bool matchBase = basePath.length() > 1;
	string token, basePathToken;
	const Directory *dir = root;
	while (sl.getNextNonEmptyToken(token))
	{
		if (matchBase)
		{
			if (slBase.getNextNonEmptyToken(basePathToken))
			{
				if (stricmp(basePathToken, token)) return nullptr;
				continue;
			}
			matchBase = false;
		}
		const Directory *nextDir = nullptr;
		for (auto j = dir->directories.cbegin(); j != dir->directories.cend(); ++j)
		{
			if (!stricmp((*j)->getName(), token))
			{
				nextDir = *j;
				break;
			}
		}
		if (!nextDir) return nullptr;
		dir = nextDir;
	}
	return dir;
}

bool DirectoryListing::spliceTree(DirectoryListing& tree, SpliceTreeResult& sr)
{
	sr.parentUserData = nullptr;
	sr.insertParent = false;
	string path = tree.getBasePath();
	string dirName;
	for (;;)
	{
		string::size_type pos = path.rfind('/');
		if (pos == string::npos) break;
		if (pos == path.length()-1)
		{
			path.erase(pos);
			continue;
		}
		dirName = path.substr(pos + 1);
		path.erase(pos);
		break;
	}
	SimpleStringTokenizer<char> sl(path, '/');
	string token;
	Directory* dir = root;
	size_t prevPos = 0;
	while (sl.getNextNonEmptyToken(token))
	{
		Directory* nextDir = nullptr;
		for (auto j = dir->directories.cbegin(); j != dir->directories.cend(); ++j)
		{
			if ((*j)->getName() == token)
			{
				nextDir = *j;
				break;
			}
		}
		if (!nextDir)
		{
			sl.reset(prevPos);
			break;
		}
		dir = nextDir;
		prevPos = sl.getPos();
	}
	bool initFirstItem = true;
	while (sl.getNextNonEmptyToken(token))
	{
		Directory* newDir = new Directory(dir, token, false);
		if (initFirstItem)
		{
			sr.parentUserData = dir->getUserData();
			sr.firstItem = newDir;
			sr.insertParent = true;
			initFirstItem = false;
		}
		if (dir == root) dir->setComplete(false);
		dir->directories.push_back(newDir);
		dir = newDir;
	}
	if (dir == root && dirName.empty())
	{
		delete root;
		root = tree.root;
		tree.root = nullptr;
		root->parent = nullptr;
		if (initFirstItem) sr.firstItem = root;
		return true;
	}
	Directory *src = tree.root;
	bool addDir = true;
	for (auto i = dir->directories.begin(); i != dir->directories.end(); i++)
		if ((*i)->getName() == dirName)
		{
			Directory* dest = *i;
			*i = tree.root;
			src->setUserData(dest->getUserData());
			delete dest;
			if (initFirstItem)
			{
				sr.parentUserData = src->getUserData();
				sr.firstItem = src;
				initFirstItem = false;
			}
			addDir = false;
			break;
		}

	src->parent = dir;
	src->name = std::move(dirName); // Partial Filelist's root has no name
	if (addDir) dir->directories.push_back(src);
	tree.root = nullptr;
	Directory::updateInfo(src);
	if (initFirstItem)
	{
		sr.parentUserData = dir->getUserData();
		sr.firstItem = src;
		sr.insertParent = true;
	}
	return true;
}

void DirectoryListing::Directory::addFile(DirectoryListing::File *f)
{
	files.push_back(f);
	f->setParent(this);
	totalSize += f->getSize();
	totalUploadCount += f->getUploadCount();
	if (f->getTS() > maxTS) maxTS = f->getTS();
	const MediaInfoUtil::Info *media = f->getMedia();
	if (media && media->bitrate)
	{
		if (media->bitrate < minBitrate) minBitrate = media->bitrate;
		if (media->bitrate > maxBitrate) maxBitrate = media->bitrate;
	}
}

DirectoryListing::SearchContext::SearchContext(): fileIndex(0), whatFound(FOUND_NOTHING),
	dir(nullptr), file(nullptr), matched{0, 0}
{
}

void DirectoryListing::SearchContext::createCopiedPath(const Directory *dir, vector<const Directory*> &srcPath)
{
	srcPath.clear();
	const Directory *srcDir = dir;
	while (srcDir)
	{
		srcPath.push_back(srcDir);
		srcDir = srcDir->getParent();
	}
	int i = (int) srcPath.size() - 1;
	int j = 0;
	while (i >= 0 && j < (int) copiedPath.size())
	{
		if (copiedPath[j].src != srcPath[i]) break;
		j++;
		i--;
	}
	dcassert(j > 0);
	copiedPath.resize(i + j + 1);
	while (i >= 0)
	{
		Directory *parent = copiedPath[j-1].copy;
		const Directory *src = srcPath[i];
		copiedPath[j].src = src;
		copiedPath[j].copy = src->clone(parent);
		copiedPath[j].copy->setFlag(src->getFlags() & DIR_STATUS_FLAGS);
		parent->directories.push_back(copiedPath[j].copy);
		j++;
		i--;
	}
}

bool DirectoryListing::SearchContext::match(const SearchQuery &sq, Directory *root, DirectoryListing *dest, vector<const Directory*> &pathCache)
{
	vector<int> tmp;

	Directory *current = root;
	whatFound = FOUND_NOTHING;
	dir = nullptr;
	file = nullptr;
	matched[0] = matched[1] = 0;

	copiedPath.clear();
	if (dest) copiedPath.emplace_back(root, dest->getRoot());
	Directory *copy = nullptr;

	tmp.push_back(-1);
	current->unsetFlag(FLAG_FOUND | FLAG_HAS_FOUND);
	while (current)
	{
		int &pos = tmp.back();
		if (pos < 0)
		{
			uint16_t dirFlags = 0;
			for (int i = 0; i < (int) current->files.size(); i++)
			{
				File *file = current->files[i];
				if (file->match(sq))
				{
					++matched[0];
					if (dest)
					{
						File *copiedFile = file->clone();
						copiedFile->setFlag(file->getFlags() & FILE_STATUS_FLAGS);
						if (!copy)
						{							
							createCopiedPath(current, pathCache);
							copy = copiedPath.back().copy;
						}
						copy->addFile(copiedFile);
					}
					file->setFlag(FLAG_FOUND);
					dirFlags = FLAG_HAS_FOUND;
					if (whatFound == FOUND_NOTHING)
					{
						fileIndex = i;
						this->file = file;
						this->dir = current;
						dirIndex = tmp;
						dirIndex.pop_back();
						whatFound = FOUND_FILE;
					}
				} else file->unsetFlag(FLAG_FOUND);
			}
			current->setFlag(dirFlags);
			pos = 0;
		}
		if (copy)
		{
			Directory::updateInfo(copy);
			copy = nullptr;
		}
		if (pos < (int) current->directories.size())
		{
			current = current->directories[pos];
			current->unsetFlag(FLAG_FOUND | FLAG_HAS_FOUND);
			if (current->match(sq))
			{
				++matched[1];
				current->setFlag(FLAG_FOUND);
				Directory *parent = current->getParent();
				parent->setFlag(FLAG_HAS_FOUND);
				if (whatFound == FOUND_NOTHING)
				{
					fileIndex = 0;
					this->file = nullptr;
					this->dir = current;
					dirIndex = tmp;
					whatFound = FOUND_DIR;
				}
			}
			tmp.push_back(-1);
			continue;
		}
		tmp.pop_back();
		if (tmp.empty()) break;
		tmp.back()++;
		Directory *parent = current->getParent();
		dcassert(parent);
		if (current->isAnySet(FLAG_HAS_FOUND)) parent->setFlag(FLAG_HAS_FOUND);
		current = parent;
	}

	return whatFound != FOUND_NOTHING;
}

bool DirectoryListing::SearchContext::goToFirstFound(const Directory *root)
{
	vector<int> tmp;
	
	const Directory *current = root;
	whatFound = FOUND_NOTHING;
	dir = nullptr;
	file = nullptr;

	tmp.push_back(-1);
	while (current)
	{
		int &pos = tmp.back();
		if (pos < 0)
		{
			for (int i = 0; i < (int) current->files.size(); i++)
			{
				const File *file = current->files[i];
				if (file->isAnySet(FLAG_FOUND))
				{
					fileIndex = i;
					this->file = file;
					this->dir = current;
					dirIndex = std::move(tmp);
					dirIndex.pop_back();
					whatFound = FOUND_FILE;
					return true;
				}
			}
			pos = 0;
		}
		if (pos < (int) current->directories.size())
		{
			current = current->directories[pos];
			tmp.push_back(-1);
			continue;
		}
		tmp.pop_back();
		if (tmp.empty()) break;
		tmp.back()++;
		current = current->getParent();
	}

	return false;
}

bool DirectoryListing::SearchContext::next()
{
	if (whatFound == FOUND_NOTHING) return false;

	vector<int> newIndex(dirIndex);
	int newFileIndex;
	const Directory *newDir = dir;
	bool searchFiles;

	if (whatFound == FOUND_DIR)
	{
		newFileIndex = 0;
		if (dir->isSet(FLAG_HAS_FOUND))
		{
			searchFiles = true;
		} else
		{
			if (newIndex.empty()) return false;
			newDir = dir->getParent();
			newIndex.back()++;
			searchFiles = false;
		}
	} else
	{
		newFileIndex = fileIndex + 1;
		searchFiles = true;
	}

	while (!newIndex.empty())
	{
		if (searchFiles)
		{
			while (newFileIndex < (int) newDir->files.size())
			{
				if (newDir->files[newFileIndex]->isSet(FLAG_FOUND))
				{
					whatFound = FOUND_FILE;
					fileIndex = newFileIndex;
					file = newDir->files[newFileIndex];
					dir = newDir;
					dirIndex = newIndex;
					return true;
				}
				newFileIndex++;
			}
			searchFiles = false;
			newIndex.push_back(0);
			newFileIndex = 0;
		}
		int newDirIndex = newIndex.back();
		while (newDirIndex < (int) newDir->directories.size())
		{
			const Directory *dirEntry = newDir->directories[newDirIndex];
			if (dirEntry->isSet(FLAG_FOUND))
			{
				whatFound = FOUND_DIR;
				fileIndex = 0;
				file = nullptr;
				dir = dirEntry;
				newIndex.back() = newDirIndex;
				dirIndex = newIndex;
				return true;
			}
			if (dirEntry->isSet(FLAG_HAS_FOUND))
			{
				searchFiles = true;
				newIndex.back() = newDirIndex;
				newDir = dirEntry;
				break;
			}
			newDirIndex++;
		}
		if (searchFiles) continue;
		newIndex.pop_back();
		if (!newIndex.empty()) newIndex.back()++;
		newDir = newDir->getParent();
	}
	return false;
}

bool DirectoryListing::SearchContext::prev()
{
	if (whatFound == FOUND_NOTHING) return false;

	vector<int> newIndex(dirIndex);
	int newFileIndex;
	const Directory *newDir = dir;
	bool searchFiles;

	if (whatFound == FOUND_DIR)
	{
		if (newIndex.empty()) return false;
		newDir = dir->getParent();
		newIndex.back()--;
		searchFiles = false;
		newFileIndex = 0;
	} else
	{
		newFileIndex = fileIndex - 1;
		searchFiles = true;
	}

	while (!newIndex.empty())
	{
		if (searchFiles)
		{
			while (newFileIndex >= 0)
			{
				if (newDir->files[newFileIndex]->isSet(FLAG_FOUND))
				{
					whatFound = FOUND_FILE;
					fileIndex = newFileIndex;
					file = newDir->files[newFileIndex];
					dir = newDir;
					dirIndex = newIndex;
					return true;
				}
				newFileIndex--;
			}
			if (newDir->isSet(FLAG_FOUND))
			{
				whatFound = FOUND_DIR;
				fileIndex = 0;
				file = nullptr;
				dir = newDir;
				dirIndex = newIndex;
				return true;
			}
			newDir = newDir->getParent();
			if (!newDir) return false;
			searchFiles = false;
			newIndex.back()--;
			newFileIndex = 0;
		}
		searchFiles = true;
		int newDirIndex = newIndex.back();
		while (newDirIndex >= 0)
		{
			const Directory *dirEntry = newDir->directories[newDirIndex];
			if (dirEntry->isSet(FLAG_HAS_FOUND))
			{
				searchFiles = false;
				newDir = dirEntry;
				newIndex.back() = newDirIndex;
				newIndex.push_back((int) newDir->directories.size() - 1);
				break;
			}
			if (dirEntry->isSet(FLAG_FOUND))
			{
				whatFound = FOUND_DIR;
				fileIndex = 0;
				file = nullptr;
				dir = dirEntry;
				newIndex.back() = newDirIndex;
				dirIndex = newIndex;
				return true;
			}
			newDirIndex--;
		}
		if (!searchFiles) continue;
		newIndex.pop_back();
		newFileIndex = (int) newDir->files.size() - 1;
	}
	return false;
}

bool DirectoryListing::SearchContext::makeIndexForFound(const Directory *dir)
{
	vector<int> newIndex;
	const Directory *parent = dir->getParent();
	while (parent)
	{
		if (!parent->isSet(FLAG_HAS_FOUND)) return false;
		bool found = false;
		for (int i = 0; i < (int) parent->directories.size(); i++)
			if (parent->directories[i] == dir)
			{
				newIndex.push_back(i);
				found = true;
				break;
			}
		if (!found) return false;
		dir = parent;
		parent = dir->getParent();
	}
	dirIndex.resize(newIndex.size());
	for (vector<int>::size_type i = 0; i < newIndex.size(); i++)
		dirIndex[i] = newIndex[newIndex.size()-1-i];
	return true;
}

bool DirectoryListing::SearchContext::setFound(const Directory *dir)
{
	if (!(dir->isSet(FLAG_FOUND))) return false;
	if (!makeIndexForFound(dir)) return false;
	fileIndex = 0;
	whatFound= FOUND_DIR;
	this->dir = dir;
	file = nullptr;
	return true;
}

bool DirectoryListing::SearchContext::setFound(const File *file)
{
	if (!(file->isSet(FLAG_FOUND))) return false;
	const Directory *dir = file->getParent();
	if (!(dir->isSet(FLAG_HAS_FOUND))) return false;
	int newFileIndex = -1;
	for (int i = 0; i < (int) dir->files.size(); i++)
		if (dir->files[i] == file)
		{
			newFileIndex = i;
			break;
		}
	if (newFileIndex < 0) return false;
	if (!makeIndexForFound(dir)) return false;
	fileIndex = newFileIndex;
	whatFound= FOUND_FILE;
	this->dir = dir;
	this->file = file;
	return true;
}

void DirectoryListing::SearchContext::clear()
{
	fileIndex = 0;
	whatFound = FOUND_NOTHING;
	dir = nullptr;
	file = nullptr;
	dirIndex.clear();
	copiedPath.clear();
	matched[0] = matched[1] = 0;
}

bool DirectoryListing::File::match(const DirectoryListing::SearchQuery &sq) const
{
	if (!size && (sq.flags & SearchQuery::FLAG_SKIP_EMPTY))
		return false;
	MaskType skipFlags = 0;
	if (sq.flags & SearchQuery::FLAG_SKIP_OWNED)
		skipFlags |= FLAG_DOWNLOADED | FLAG_SHARED;
	if (sq.flags & SearchQuery::FLAG_SKIP_CANCELED)
		skipFlags |= FLAG_CANCELED;
	if (getFlags() & skipFlags)
		return false;
	if (sq.flags & SearchQuery::FLAG_TYPE)
	{
		if (sq.type == FILE_TYPE_DIRECTORY) return false;
		if (sq.type == FILE_TYPE_TTH)
		{
			if (tthRoot != sq.tth) return false;
		}
		else
		{
			unsigned fileTypes = getFileTypesFromFileName(name);
			if (!(fileTypes & 1<<sq.type)) return false;
		}
	}
	if (sq.flags & SearchQuery::FLAG_STRING)
	{
		if (name.find(sq.text) == string::npos) return false;
	}
	if (sq.flags & SearchQuery::FLAG_WSTRING)
	{
		Text::utf8ToWide(name, sq.wbuf);
		if (!(sq.flags & SearchQuery::FLAG_MATCH_CASE)) Text::makeLower(sq.wbuf);
		if (sq.wbuf.find(sq.wtext) == wstring::npos) return false;
	}
	if (sq.flags & SearchQuery::FLAG_REGEX)
	{
		if (!std::regex_search(name, sq.re)) return false;
	}
	if (sq.flags & SearchQuery::FLAG_SIZE)
	{
		if (size < sq.minSize || size > sq.maxSize) return false;
	}
	if (sq.flags & SearchQuery::FLAG_TIME_SHARED)
	{
		if (getTS() < sq.minSharedTime) return false;
	}
	return true;
}

bool DirectoryListing::Directory::match(const DirectoryListing::SearchQuery &sq) const
{
	if (sq.flags & SearchQuery::FLAG_TYPE)
	{
		if (sq.type != FILE_TYPE_DIRECTORY) return false;
	}
	if (sq.flags & (SearchQuery::FLAG_SIZE | SearchQuery::FLAG_TIME_SHARED))
		return false;
	if (sq.flags & SearchQuery::FLAG_STRING)
	{
		if (name.find(sq.text) == string::npos) return false;
	}
	if (sq.flags & SearchQuery::FLAG_WSTRING)
	{
		Text::utf8ToWide(name, sq.wbuf);
		if (!(sq.flags & SearchQuery::FLAG_MATCH_CASE)) Text::makeLower(sq.wbuf);
		if (sq.wbuf.find(sq.wtext) == wstring::npos) return false;
	}
	if (sq.flags & SearchQuery::FLAG_REGEX)
	{
		if (!std::regex_search(name, sq.re)) return false;
	}
	return true;
}

void DirectoryListing::markAsFound(File* file) noexcept
{
	file->setFlag(FLAG_FOUND);
	Directory* d = file->getParent();
	while (d)
	{
		if (d->isAnySet(FLAG_HAS_FOUND)) break;
		d->setFlag(FLAG_HAS_FOUND);
		d = d->getParent();
	}
}

void DirectoryListing::addDclstSelf(const string& filePath, std::atomic_bool& stopFlag)
{
	if (!root || root->directories.empty()) return;
	Directory* d = root->directories.front();
	TigerTree tree;
	if (!Util::getTTH(filePath, true, 512 * 1024, stopFlag, tree))
		throw AbortException("Unable to get TTH of " + filePath);
	if (stopFlag)
		throw AbortException("DirectoryListing::addDcLstSelf - " + STRING(ABORT_EM));
	File* f = new File(nullptr, Util::getFileName(filePath), tree.getFileSize(), tree.getRoot(), 0, 0, nullptr);
	f->setFlag(FLAG_DCLST_SELF);
	d->addFile(f);
	d->setFlag(FLAG_HAS_OTHER);
}

void DirectoryListing::getFileParams(const File* f, StringMap& ucParams) const noexcept
{
	ucParams["type"] = "File";
	string s = getPath(f) + f->getName();
	ucParams["fileFN"] = s;
	ucParams["file"] = s; // Compatibility alias
	s = Util::toString(f->getSize());
	ucParams["fileSI"] = s;
	ucParams["filesize"] = s; // Compatibility alias
	s = Util::formatBytes(f->getSize());
	ucParams["fileSIshort"] = s;
	ucParams["filesizeshort"] = s; // Compatibility alias
	s = f->getTTH().toBase32();
	ucParams["fileTR"] = s;
	ucParams["tth"] = s; // Compatibility alias
}

void DirectoryListing::getDirectoryParams(const Directory* d, StringMap& ucParams) const noexcept
{
	ucParams["type"] = "Directory";
	string s = getPath(d) + d->getName();
	ucParams["fileFN"] = s;
	ucParams["file"] = s; // Compatibility alias
	s = Util::toString(d->getTotalSize());
	ucParams["fileSI"] = s;
	ucParams["filesize"] = s; // Compatibility alias
	s = Util::formatBytes(d->getTotalSize());
	ucParams["fileSIshort"] = s;
	ucParams["filesizeshort"] = s; // Compatibility alias
}

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
#include "CFlylinkDBManager.h"
#include "SimpleStringTokenizer.h"
#include "SimpleXML.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#include "SimpleXMLReader.h"
#include "User.h"
#include "ShareManager.h"

static const int PROGRESS_REPORT_TIME = 2000;

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

DirectoryListing::DirectoryListing(std::atomic_bool& abortFlag, bool createRoot) :
	abortFlag(abortFlag),
	includeSelf(false), ownList(false), incomplete(false), aborted(false)
{
	root = createRoot ? new Directory(nullptr, Util::emptyString, false, true) : nullptr;
	tthSet = createRoot ? nullptr : new TTHMap;
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
		auto user = std::make_shared<User>(CID(), name, 0);
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
			Encoder::fromBase32(name.c_str() + i + 1, cid.writableData(), CID::SIZE, &hasCID);
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
		return ClientManager::getUser(name, "Unknown Hub", 0);
	
	return ClientManager::createUser(cid, name, 0);
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
		           const UserPtr& user, bool ownList)
			: list(list), current(root), inListing(false), ownList(ownList),
			  emptyFileNameCounter(0), progressNotif(nullptr), stream(is), filesProcessed(0)
		{
			nextProgressReport = GET_TICK() + PROGRESS_REPORT_TIME;
			list->basePath = "/";
		}
		
		~ListLoader() { }
		
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
		
		bool inListing;
		bool ownList;
		unsigned emptyFileNameCounter;
		
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
	ListLoader ll(this, is, getRoot(), getUser(), ownList);
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
				if (attrib == attrShared) valShared = &value;
			}
			
			if (!valFilename) return;
			
			if (!valSize) return;
			
			if (!valTTH || valTTH->length() != 39) return;
			
			TTHValue tth;
			bool error;
			Encoder::fromBase32(valTTH->c_str(), tth.data, sizeof(tth.data), &error);
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
					shared = val / 10000000;
			} else
			if (valTS)
			{
				int64_t val = Util::toInt64(*valTS);
				if (val > 0)
					shared = val;
			}

			uint32_t hit = 0;
			if (valHit)
				hit = Util::toUInt32(*valHit);

			DirectoryListing::MediaInfo tempMedia;
			DirectoryListing::MediaInfo *media = nullptr;
			
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
						tempMedia.width = atoi(valWH->c_str());
						tempMedia.height = atoi(valWH->c_str() + pos + 1);
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

			auto f = new DirectoryListing::File(current, *valFilename, size, tth, hit, shared, media);
			current->files.push_back(f);
			current->totalSize += size;
			current->totalHits += hit;
			if (shared > current->maxTS) current->maxTS = shared;
			if (media && media->bitrate)
			{
				if (media->bitrate < current->minBitrate) current->minBitrate = media->bitrate;
				if (media->bitrate > current->maxBitrate) current->maxBitrate = media->bitrate;
			}
			
			if (size && !ownList)
			{
				if (QueueManager::isQueued(f->getTTH()))
				{
					f->setFlag(DirectoryListing::FLAG_QUEUED);
					current->setFlag(DirectoryListing::FLAG_HAS_QUEUED);
				}
#if 0 // FIXME
				if (ShareManager::isTTHShared(f->getTTH()))
				{
					f->setFlag(DirectoryListing::FLAG_SHARED);
					current->setFlag(DirectoryListing::FLAG_HAS_SHARED);
				}
#endif
				unsigned flags;
				string path;
				CFlylinkDBManager::getInstance()->getFileInfo(f->getTTH(), flags, path);
				if (flags & CFlylinkDBManager::FLAG_SHARED)
				{
					f->setFlag(DirectoryListing::FLAG_SHARED);
					f->setPath(path);
					current->setFlag(DirectoryListing::FLAG_HAS_SHARED);
				} else
				if (flags & CFlylinkDBManager::FLAG_DOWNLOADED)
				{
					f->setFlag(DirectoryListing::FLAG_DOWNLOADED);
					f->setPath(path);
					current->setFlag(DirectoryListing::FLAG_HAS_DOWNLOADED);
				} else
				if (flags & CFlylinkDBManager::FLAG_DOWNLOAD_CANCELED)
				{
					f->setFlag(DirectoryListing::FLAG_CANCELED);
					current->setFlag(DirectoryListing::FLAG_HAS_CANCELED);
				} else current->setFlag(DirectoryListing::FLAG_HAS_OTHER);
			}

			fileProcessed();
		}
		else if (name == tagDirectory)
		{
			if (!list->tthSet)
			{
				const string &fileName = getAttrib(attribs, attrName, 0);
				const bool incomp = getAttrib(attribs, attrIncomplete, 1) == "1";
				DirectoryListing::Directory* d;

				if (fileName.empty())
					d = new DirectoryListing::Directory(current, "empty_file_name_" + Util::toString(++emptyFileNameCounter), false, !incomp);
				else
					d = new DirectoryListing::Directory(current, fileName, false, !incomp);
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
					UserPtr user = ClientManager::createUser(cid, "", 0);
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
		const wchar_t *aName = static_cast<wchar_t*>(a->getUserData());
		const wchar_t *bName = static_cast<wchar_t*>(b->getUserData());
		return Util::defaultSort(aName, bName, false) < 0;
	}
};

template<typename T>
static void sortList(vector<T*> &data)
{
	for (auto it = data.begin(); it != data.end(); it++)
	{
		T *item = *it;
		const string &name = item->getName();
		wchar_t *wname = utf8ToWidePtr(name);
		item->setUserData(wname);
	}
	sort(data.begin(), data.end(), SortFunc<T>());
	for (auto it = data.begin(); it != data.end(); it++)
	{
		T *item = *it;
		wchar_t *wname = static_cast<wchar_t*>(item->getUserData());
		delete[] wname;
		item->setUserData(nullptr);
	}
}

void ListLoader::endTag(const string& name, const string&)
{
	if (!inListing) return;
	notifyProgress();
	if (name == tagDirectory)
	{
		if (current)
		{
			Flags::MaskType addFlags = 0;
			current->updateSubDirs(addFlags);
			current->setFlag(addFlags);
			sortList(current->files);
			sortList(current->directories);
			current = current->getParent();
		}
	}
	else if (name == tagFileListing)
	{
		if (current)
		{
			Flags::MaskType unused = 0;
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
	dir.append(1, '\\');
	
	const Directory* cur = d->getParent();
	while (cur != root)
	{
		dir.insert(0, cur->getName() + '\\');
		cur = cur->getParent();
	}
	return dir;
}

void DirectoryListing::download(const Directory* aDir, const string& aTarget, bool highPrio, QueueItem::Priority prio, bool p_first_file)
{
	string target = (aDir == getRoot()) ? aTarget : aTarget + aDir->getName() + PATH_SEPARATOR;
	if (!aDir->getComplete())
	{
		// folder is not completed (partial list?), so we need to download it first
		QueueManager::getInstance()->addDirectory(Util::emptyString, hintedUser, target, prio);
	}
	else
	{
		// First, recurse over the directories
		const Directory::List& lst = aDir->directories;
		//[!] sort(lst.begin(), lst.end(), Directory::DirSort()); //[-] FlylinkDC++ Team - пусть качаются диры в порядке файл-листа.
		for (auto j = lst.cbegin(); j != lst.cend(); ++j)
		{
			download(*j, target, highPrio, prio, p_first_file);
			p_first_file = false;
		}
		// Then add the files
		const File::List& l = aDir->files;
		//[!] sort(l.begin(), l.end(), File::FileSort());  //[-] FlylinkDC++ Team - сортировка файлов по алфавиту тормозит при кол-ва файлов > 10 тыс
		for (auto i = l.cbegin(); i != l.cend(); ++i)
		{
			const File* file = *i;
			try
			{
				download(file, target + file->getName(), false, highPrio, prio, false, p_first_file);
				p_first_file = false;
			}
			catch (const QueueException& e)
			{
				LogManager::message("DirectoryListing::download - QueueException:" + e.getError());
			}
			catch (const FileException& e)
			{
				LogManager::message("DirectoryListing::download - FileException:" + e.getError());
			}
		}
	}
}

#if 0
void DirectoryListing::download(const string& aDir, const string& aTarget, bool highPrio, QueueItem::Priority prio)
{
	if (aDir.size() <= 2)
	{
		LogManager::message("[error] DirectoryListing::download aDir.size() <= 2 aDir=" + aDir + " aTarget = " + aTarget);
		return;
	}
	dcassert(aDir.size() > 2);
	dcassert(aDir[aDir.size() - 1] == '\\'); // This should not be PATH_SEPARATOR
	Directory* d = find(aDir, getRoot());
	if (d != nullptr)
		download(d, aTarget, highPrio, prio);
}
#endif

void DirectoryListing::download(const File* aFile, const string& aTarget, bool view, bool highPrio, QueueItem::Priority prio, bool p_isDCLST, bool p_first_file)
{
	const Flags::MaskType flags = (Flags::MaskType)(view ? ((p_isDCLST ? QueueItem::FLAG_DCLST_LIST : QueueItem::FLAG_TEXT) | QueueItem::FLAG_CLIENT_VIEW) : 0);
	try
	{
		QueueManager::getInstance()->add(aTarget, aFile->getSize(), aFile->getTTH(), getUser(), flags, true, p_first_file); // TODO
	}
	catch (const Exception& e)
	{
		LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
	}
	
	if (highPrio || (prio != QueueItem::DEFAULT))
	{
		QueueManager::getInstance()->setPriority(aTarget, highPrio ? QueueItem::HIGHEST : prio);
	}
}

#if 0
DirectoryListing::Directory* DirectoryListing::find(const string& aName, Directory* current)
{
	string::size_type end = aName.find('\\');
	dcassert(end != string::npos);
	if (end != string::npos)
	{
		const string name = aName.substr(0, end);
		auto i = std::find(current->directories.begin(), current->directories.end(), name);
		if (i != current->directories.end())
		{
			if (end == (aName.size() - 1))
				return *i;
			else
				return find(aName.substr(end + 1), *i);
		}
	}
	return nullptr;
}
#endif

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

void DirectoryListing::Directory::updateSubDirs(Flags::MaskType& updatedFlags)
{
	Flags::MaskType flags = 0;
	for (auto i = directories.cbegin(); i != directories.cend(); i++)
	{
		const DirectoryListing::Directory *dir = *i;
		if (dir->getAdls()) continue;
		flags |= dir->getFlags() &
			(DirectoryListing::FLAG_HAS_SHARED | DirectoryListing::FLAG_HAS_DOWNLOADED |
			DirectoryListing::FLAG_HAS_CANCELED | DirectoryListing::FLAG_HAS_OTHER);
		totalFileCount += dir->totalFileCount;
		totalDirCount += dir->totalDirCount;
		totalSize += dir->totalSize;
		totalHits += dir->totalHits;
		if (dir->maxTS > maxTS) maxTS = dir->maxTS;
		if (dir->minBitrate < minBitrate) minBitrate = dir->minBitrate;
		if (dir->maxBitrate > maxBitrate) maxBitrate = dir->maxBitrate;
		totalDirCount++;
	}
	totalFileCount += files.size();
	updatedFlags |= flags;
}

void DirectoryListing::Directory::updateFiles(Flags::MaskType& updatedFlags)
{
	Flags::MaskType flags = 0;
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
		totalHits += file->getHit();
		auto shared = file->getTS();
		if (shared > maxTS) maxTS = shared;
		const MediaInfo *media = file->getMedia();
		if (media && media->bitrate)
		{
			if (media->bitrate < minBitrate) minBitrate = media->bitrate;
			if (media->bitrate > maxBitrate) maxBitrate = media->bitrate;
		}
	}
	updatedFlags |= flags;
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
		auto prevTotalHits = dir->totalHits;
		auto prevMinBitrate = dir->minBitrate;
		auto prevMaxBitrate = dir->maxBitrate;
		
		dir->totalFileCount = 0;
		dir->totalDirCount = 0;
		dir->totalSize = 0;
		dir->maxTS = 0;
		dir->totalHits = 0;
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
			dir->totalHits != prevTotalHits ||
			dir->maxBitrate != prevMaxBitrate ||
			dir->minBitrate != prevMinBitrate ||
			dir->getFlags() != flags;
		if (dir->getParent()) dir->setFlags(flags);
		if (!update) break;
	}
}

void DirectoryListing::Directory::checkDupes(const DirectoryListing* lst)
{
#if 0
	Flags::MaskType result = 0;
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		(*i)->checkDupes(lst);
		result |= (*i)->getFlags() & (
		              FLAG_OLD_TTH | FLAG_DOWNLOAD | FLAG_SHARED | FLAG_NOT_SHARED | FLAG_QUEUE);  // TODO | FLAG_VIRUS_FILE
	}
	if (files.size())
		result |= FLAG_DOWNLOAD_FOLDER;
	for (auto i = files.cbegin(); i != files.cend(); ++i)
	{
		//don't count 0 byte m_files since it'll give lots of partial dupes
		//of no interest
		if ((*i)->getSize() > 0)
		{
			result |= (*i)->getFlags() & (
			              FLAG_OLD_TTH | FLAG_DOWNLOAD | FLAG_SHARED | FLAG_NOT_SHARED | FLAG_QUEUE);
			if (!(*i)->isAnySet(FLAG_OLD_TTH | FLAG_DOWNLOAD | FLAG_SHARED | FLAG_QUEUE))
			{
				result &= ~FLAG_DOWNLOAD_FOLDER;
			}
		}
	}
	setFlags(result);
#endif
}

// !SMT!-UI
void DirectoryListing::checkDupes()
{
	root->checkDupes(this);
}

DirectoryListing::Directory* DirectoryListing::findDirPath(const string& path) const
{
	if (!root) return nullptr;
	SimpleStringTokenizer<char> sl(path, '/');
	string token;
	const Directory *dir = root;
	while (sl.getNextNonEmptyToken(token))
	{
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

bool DirectoryListing::spliceTree(DirectoryListing::Directory* dest, DirectoryListing& tree)
{
	if (!dest) return false;
	Directory *parent = dest->getParent();
	if (!parent)
	{
		dcassert(dest == root);
		delete root;
		root = tree.root;
		tree.root = nullptr;
		root->parent = nullptr;
		return true;
	}
	for (auto i = parent->directories.begin(); i != parent->directories.end(); i++)
		if (*i == dest)
		{
			*i = tree.root;
			Directory *src = tree.root;
			src->parent = parent;
			src->name = std::move(dest->name); // Partial Filelist's root has no name
			delete dest;
			tree.root = nullptr;
			Directory::updateInfo(src);
			return true;
		}
	return false;
}

void DirectoryListing::Directory::addFile(DirectoryListing::File *f)
{
	files.push_back(f);
	totalSize += f->getSize();
	totalHits += f->getHit();
	if (f->getTS() > maxTS) maxTS = f->getTS();
	const MediaInfo *media = f->getMedia();
	if (media && media->bitrate)
	{
		if (media->bitrate < minBitrate) minBitrate = media->bitrate;
		if (media->bitrate > maxBitrate) maxBitrate = media->bitrate;
	}
}

DirectoryListing::SearchContext::SearchContext(): fileIndex(0), whatFound(FOUND_NOTHING),
	dir(nullptr), file(nullptr)
{
}

void DirectoryListing::SearchContext::createCopiedPath(const Directory *dir)
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
		copiedPath[j].copy = new Directory(parent, src->getName(), src->getAdls(), true);
		copiedPath[j].copy->setFlag(src->getFlags() & (FLAG_HAS_QUEUED | FLAG_HAS_SHARED | FLAG_HAS_DOWNLOADED | FLAG_HAS_CANCELED | FLAG_HAS_OTHER));
		parent->directories.push_back(copiedPath[j].copy);
		j++;
		i--;
	}
}

bool DirectoryListing::SearchContext::match(const SearchQuery &sq, Directory *root, DirectoryListing *dest)
{
	vector<int> tmp;
	
	Directory *current = root;
	whatFound = FOUND_NOTHING;
	dir = nullptr;
	file = nullptr;

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
			Flags::MaskType dirFlags = 0;
			for (int i = 0; i < (int) current->files.size(); i++)
			{
				File *file = current->files[i];
				if (file->match(sq))
				{
					if (dest)
					{
						File *copiedFile = new File(*file);
						copiedFile->setFlag(file->getFlags() & (FLAG_QUEUED | FLAG_SHARED | FLAG_DOWNLOADED | FLAG_CANCELED));
						if (!copy)
						{							
							createCopiedPath(current);
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
		if (pos < (int) current->directories.size())
		{
			current = current->directories[pos];
			current->unsetFlag(FLAG_FOUND | FLAG_HAS_FOUND);
			if (current->match(sq))
			{
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
		if (copy)
		{
			Directory::updateInfo(copy);
			copy = nullptr;
		}
	}

	return whatFound != FOUND_NOTHING;
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
	srcPath.clear();
}

bool DirectoryListing::File::match(const DirectoryListing::SearchQuery &sq) const
{
	if (sq.flags & SearchQuery::FLAG_TYPE)
	{
		if (sq.type == FILE_TYPE_DIRECTORY) return false;
		unsigned fileTypes = getFileTypesFromFileName(name);
		if (!(fileTypes & 1<<sq.type)) return false;
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

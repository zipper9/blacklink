#ifndef SHARED_FILE_H_
#define SHARED_FILE_H_

#include "Util.h"

class SharedDir;
class SharedFile;

class BaseDirItem
{       	
	public:
		enum
		{
			FLAG_NOT_FOUND     = 1,
			FLAG_SHARE_REMOVED = 2,
			FLAG_SHARE_LOST    = 4,
			FLAG_HASH_FILE     = 8
		};		
	
	protected:
		string name;
		string lowerName;

	public:
		const string& getLowerName() const
		{
			return lowerName.empty() ? name : lowerName;
		}
		const string& getName() const
		{
			return name;
		}
		void setName(const string& name)
		{
			this->name = name;
			Text::toLower(name, lowerName);
		}
};

typedef std::shared_ptr<SharedFile> SharedFilePtr;

class SharedFile: public BaseDirItem
{
		friend class ShareManager;
		friend class SharedDir;
	
	public:		
		SharedFile(const string& name, const TTHValue& root, int64_t size, uint64_t timestamp, uint64_t timeShared, uint16_t typesMask, unsigned hit) :
			tth(root), size(size), timestamp(timestamp), timeShared(timeShared), typesMask(typesMask), hit(hit), flags(0)
		{
			dcassert(name.find('\\') == string::npos);
			setName(name);
		}

		SharedFile(const string& name, const string& lowerName, int64_t size, uint64_t timestamp, uint16_t typesMask) :
			size(size), timestamp(timestamp), timeShared(0), typesMask(typesMask), hit(0), flags(0)
		{
			dcassert(name.find('\\') == string::npos);
			this->name = name;
			this->lowerName = lowerName;
		}

		typedef boost::unordered_map<string, SharedFilePtr> FileMap;
	
	private:
		int64_t size;
		const uint16_t typesMask;
		TTHValue tth;
		uint16_t flags;
		uint64_t timestamp;
		uint64_t timeShared;
		unsigned hit;

	public:
		uint16_t getFileTypes() const { return typesMask; }
		bool hasType(int type) const noexcept;
		const TTHValue& getTTH() const { return tth; }
		int64_t getSize() const { return size; }
		void incHit() { ++hit; }
};

class SharedDir: public BaseDirItem
{
		friend class ShareManager;
		friend class ShareLoader;
	
	public:
		SharedDir(const string& name, SharedDir* parent): parent(parent), totalSize(0), filesTypesMask(0), dirsTypesMask(0), flags(0)
		{
			setName(name);
		}
		typedef std::map<string, SharedDir*> DirectoryMap;

	private:
		SharedDir* parent;
		SharedFile::FileMap files;
		DirectoryMap dirs;
		int64_t totalSize;
		uint16_t filesTypesMask;
		uint16_t dirsTypesMask;
		uint16_t flags;

	public:
		bool hasType(int type) const noexcept;
		void addTypes(uint16_t filesMask, uint16_t dirsMask) noexcept;
		void updateTypes(uint16_t filesMask, uint16_t dirsMask) noexcept;
		uint16_t getTypes() const noexcept { return filesTypesMask | dirsTypesMask; }
		void updateSize(int64_t deltaSize) noexcept;
		SharedDir* getParent() { return parent; }
		const SharedDir* getParent() const { return parent; }
		static void deleteTree(SharedDir* root);
		static SharedDir* copyTree(const SharedDir* root);
};

#endif // SHARED_FILE_H_

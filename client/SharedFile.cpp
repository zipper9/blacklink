#include "stdinc.h"
#include "SharedFile.h"
#include "FileTypes.h"

bool SharedFile::hasType(int type) const noexcept
{
	return type == FILE_TYPE_ANY || (typesMask & 1<<type) != 0;
}

bool SharedDir::hasType(int type) const noexcept
{
	return type == FILE_TYPE_ANY || ((filesTypesMask | dirsTypesMask) & 1<<type) != 0;
}

void SharedDir::addTypes(uint16_t filesMask, uint16_t dirsMask) noexcept
{
	if ((filesTypesMask | filesMask) == filesTypesMask &&
	    (dirsTypesMask | dirsMask) == dirsTypesMask) return;
	filesTypesMask |= filesMask;
	dirsTypesMask |= dirsMask;
	uint16_t mask = filesMask | dirsMask;
	SharedDir* dir = parent;
	while (dir)
	{
		if ((dir->dirsTypesMask | mask) == dir->dirsTypesMask) break;
		dir->dirsTypesMask |= mask;
		dir = dir->parent;
	}
}

void SharedDir::updateTypes(uint16_t filesMask, uint16_t dirsMask) noexcept
{
	if (filesTypesMask == filesMask && dirsTypesMask == dirsMask) return;
	filesTypesMask = filesMask;
	dirsTypesMask = dirsMask;
	SharedDir* dir = parent;
	while (dir)
	{
		uint16_t newMask = 0;
		for (auto i = dir->dirs.cbegin(); i != dir->dirs.cend(); ++i)
			newMask |= i->second->getTypes();
		if (newMask == dir->dirsTypesMask) break;
		dir->dirsTypesMask = newMask;
		dir = dir->parent;
	}
}

void SharedDir::updateSize(int64_t deltaSize) noexcept
{
	SharedDir* dir = this;
	while (dir)
	{
		dir->totalSize += deltaSize;
		dir = dir->parent;
	}
}

void SharedDir::deleteTree(SharedDir* root)
{
	if (!root) return;
	for (auto i = root->dirs.begin(); i != root->dirs.end(); ++i)
		deleteTree(i->second);
	delete root;
}

SharedDir* SharedDir::copyTree(const SharedDir* root)
{
	SharedDir* newRoot = new SharedDir(root->getName(), nullptr);
	newRoot->dirs = root->dirs;
	newRoot->totalSize = root->totalSize;
	newRoot->filesTypesMask = root->filesTypesMask;
	newRoot->dirsTypesMask = root->dirsTypesMask;
	for (auto i = root->files.cbegin(); i != root->files.cend(); i++)
	{
		const SharedFilePtr& file = i->second;
		if (!(file->flags & FLAG_HASH_FILE))
			newRoot->files.insert(make_pair(file->getLowerName(), file));
	}
	for (auto i = newRoot->dirs.begin(); i != newRoot->dirs.end(); ++i)
	{
		SharedDir* dir = copyTree(i->second);
		dir->parent = newRoot;
		i->second = dir;
	}
	return newRoot;
}

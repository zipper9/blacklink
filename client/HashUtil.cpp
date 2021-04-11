#include "stdinc.h"
#include "HashUtil.h"
#include "File.h"

bool Util::getTTH(const string& filename, bool isAbsPath, size_t bufSize, std::atomic_bool& stopFlag, TigerTree& tree, unsigned maxLevels)
{
	unique_ptr<uint8_t[]> buf(new uint8_t[bufSize]);
	try
	{
		File f(filename, File::READ, File::OPEN, isAbsPath);
		int64_t blockSize = maxLevels ? TigerTree::calcBlockSize(f.getSize(), maxLevels) : TigerTree::getMaxBlockSize(f.getSize());
		tree.setBlockSize(blockSize);
		if (f.getSize() > 0)
		{
			size_t n;
			while ((n = f.read(buf.get(), bufSize)) != 0)
			{
				tree.update(buf.get(), n);
				if (stopFlag.load())
				{
					f.close();
					tree = TigerTree(tree.getFileSize(), tree.getBlockSize(), TTHValue());
					return false;
				}
			}
		}
		f.close();
		tree.finalize();
		return true;
	}
	catch (const FileException&) {}
	return false;
}

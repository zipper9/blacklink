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


#ifndef DCPLUSPLUS_DCPP_Z_UTILS_H
#define DCPLUSPLUS_DCPP_Z_UTILS_H

#include <zlib.h>
#include <string>

class ZFilter
{
	public:
		ZFilter();
		~ZFilter();
		/**
		 * Compress data.
		 * @param in Input data
		 * @param insize Input size (Set to 0 to indicate that no more data will follow)
		 * @param out Output buffer
		 * @param outsize Output size, set to compressed size on return.
		 * @return True if there's more processing to be done
		 */
		bool operator()(const void* in, size_t& insize, void* out, size_t& outsize);

	private:
		z_stream zs;
		int64_t totalIn;
		int64_t totalOut;
		bool compressing;

		void updateSize(size_t& insize, size_t& outsize);
};

class UnZFilter
{
	public:
		UnZFilter();
		~UnZFilter();
		/**
		 * Decompress data.
		 * @param in Input data
		 * @param insize Input size (Set to 0 to indicate that no more data will follow)
		 * @param out Output buffer
		 * @param outsize Output size, set to decompressed size on return.
		 * @return True if there's more processing to be done
		 */
		bool operator()(const void* in, size_t& insize, void* out, size_t& outsize);
	private:
		z_stream zs;
};

namespace GZip
{
	void decompress(const std::string& gzipPath, const std::string &outputPath);
}

#endif // DCPLUSPLUS_DCPP_Z_UTILS_H

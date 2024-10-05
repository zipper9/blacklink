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

#ifndef DCPLUSPLUS_DCPP_FILTERED_FILE_H
#define DCPLUSPLUS_DCPP_FILTERED_FILE_H

#include "BaseStreams.h"

template<class Filter>
class FilteredInOutStream
{
	public:
		FilteredInOutStream(): more(true), buf(new uint8_t[BUF_SIZE])
		{
		}

		const Filter& getFilter() const { return filter; }
		Filter& getFilter() { return filter; }

	protected:
		static const size_t BUF_SIZE = 256 * 1024;
		Filter filter;
		std::unique_ptr<uint8_t[]> buf;
		bool more;
};

template<class Filter, bool manage>
class FilteredOutputStream : public OutputStream, public FilteredInOutStream<Filter>
{
	public:
		using OutputStream::write;
		
		explicit FilteredOutputStream(OutputStream* f) : f(f), flushed(false) { }
		~FilteredOutputStream()
		{
			if (manage) delete f;
		}
		
		size_t flushBuffers(bool force) override
		{
			if (flushed)
				return 0;
				
			flushed = true;
			size_t written = 0;
			
			for (;;)
			{
				size_t n = BUF_SIZE;
				size_t zero = 0;
				more = filter(nullptr, zero, buf.get(), n);
				
				written += f->write(buf.get(), n);
				
				if (!more)
					break;
			}
			return written + f->flushBuffers(force);
		}
		
		size_t write(const void* wbuf, size_t len) override
		{
			dcassert(len > 0);
			if (flushed)
				throw Exception("No filtered writes after flush");
				
			uint8_t* wb = (uint8_t*)wbuf;
			size_t written = 0;
			while (len > 0)
			{
				size_t n = BUF_SIZE;
				size_t m = len;
				
				more = filter(wb, m, buf.get(), n);
				wb += m;
				len -= m;
				if (n > 0)
					written += f->write(buf.get(), n);
				
				if (!more)
				{
					if (len > 0)
					{
						throw Exception("Garbage data after end of stream");
					}
					return written;
				}
			}
			return written;
		}

		bool eof() const override
		{
			return !more;
		}

	private:
		OutputStream* const f;
		bool flushed;

		using FilteredInOutStream<Filter>::BUF_SIZE;
		using FilteredInOutStream<Filter>::more;
		using FilteredInOutStream<Filter>::buf;
		using FilteredInOutStream<Filter>::filter;
};

template<class Filter, bool managed>
class FilteredInputStream : public InputStream, protected FilteredInOutStream<Filter>
{
	public:
		explicit FilteredInputStream(InputStream* f) : f(f), pos(0), valid(0), totalRead(0) { }
		~FilteredInputStream()
		{
			if (managed) delete f;
		}
		
		/**
		* Read data through filter, keep calling until len returns 0.
		* @param rbuf Data buffer
		* @param len Buffer size on entry, bytes actually read on exit
		* @return Length of data in buffer
		*/
		size_t read(void* rbuf, size_t& len) override
		{
			uint8_t* rb = static_cast<uint8_t*>(rbuf);
			
			size_t bytesRead = 0;
			size_t bytesProduced = 0;
			
			while (more && bytesProduced < len)
			{
				size_t curRead = BUF_SIZE;
				if (valid == 0)
				{
					dcassert(pos == 0);
					valid = f->read(buf.get(), curRead);
					bytesRead += curRead;
					totalRead += curRead;
				}
				
				size_t n = len - bytesProduced;
				size_t m = valid - pos;
				more = filter(&buf[pos], m, rb, n);
				pos += m;
				if (pos == valid)
				{
					valid = pos = 0;
				}
				bytesProduced += n;
				rb += n;
			}
			len = bytesRead;
			return bytesProduced;
		}

		int64_t getInputSize() const override { return f->getInputSize(); }
		int64_t getTotalRead() const override { return totalRead + pos; }

		void closeStream() override
		{
			f->closeStream();
		}

	private:
		InputStream* const f;
		size_t pos;
		size_t valid;
		int64_t totalRead;

		using FilteredInOutStream<Filter>::BUF_SIZE;
		using FilteredInOutStream<Filter>::more;
		using FilteredInOutStream<Filter>::buf;
		using FilteredInOutStream<Filter>::filter;
};

#endif // !defined(FILTERED_FILE_H)

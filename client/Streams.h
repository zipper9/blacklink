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


#ifndef DCPLUSPLUS_DCPP_STREAMS_H
#define DCPLUSPLUS_DCPP_STREAMS_H

#include "Exception.h"

#ifndef NO_RESOURCE_MANAGER
#include "ResourceManager.h"
#endif

STANDARD_EXCEPTION(FileException);

/**
 * A simple output stream. Intended to be used for nesting streams one inside the other.
 */
class OutputStream
{
	public:
		OutputStream() {}
		virtual ~OutputStream() {}
		
		/**
		 * @return The actual number of bytes written. len bytes will always be
		 *         consumed, but fewer or more bytes may actually be written,
		 *         for example if the stream is being compressed.
		 */
		virtual size_t write(const void* buf, size_t len) = 0;
		/**
		* This must be called before destroying the object to make sure all data
		* is properly written (we don't want destructors that throw exceptions
		* and the last flush might actually throw). Note that some implementations
		* might not need it...
		*
		* If force is false, only data that is subject to be deleted otherwise will be flushed.
		* This applies especially for files for which the operating system should generally decide
		* when the buffered data is flushed on disk.
		*/
		
		virtual size_t flushBuffers(bool force) = 0;
		/* This only works for file streams */
		virtual void setPos(int64_t /*pos*/) { }
		
		/**
		 * @return True if stream is at expected end
		 */
		virtual bool eof() const
		{
			return false;
		}
		
		size_t write(const string& str)
		{
			return write(str.c_str(), str.size());
		}

		OutputStream(const OutputStream &) = delete;
		OutputStream& operator= (const OutputStream &) = delete;
};

class InputStream
{
	public:
		InputStream() {}
		virtual ~InputStream() {}
		/**
		 * Call this function until it returns 0 to get all bytes.
		 * @return The number of bytes read. len reflects the number of bytes
		 *         actually read from the stream source in this call.
		 */
		virtual size_t read(void* buf, size_t& len) = 0;
		/* This only works for file streams */
		virtual void setPos(int64_t /*pos*/) { }
		
		virtual void closeStream() {}

		virtual int64_t getInputSize() const { return -1; }
		virtual int64_t getTotalRead() const { return -1; }

		InputStream(const InputStream &) = delete;
		InputStream& operator= (const InputStream &) = delete;
};

class MemoryInputStream : public InputStream
{
	public:
		MemoryInputStream(const uint8_t* src, size_t len) : pos(0), bufSize(len), buf(new uint8_t[len])
		{
			memcpy(buf, src, len);
		}
		explicit MemoryInputStream(const string& src) : pos(0), bufSize(src.size()), buf(src.size() ? new uint8_t[src.size()] : nullptr)
		{
			dcassert(src.size());
			memcpy(buf, src.data(), src.size());
		}
		
		~MemoryInputStream()
		{
			delete[] buf;
		}
		
		size_t read(void* tgt, size_t& len) override
		{
			len = min(len, bufSize - pos);
			memcpy(tgt, buf + pos, len);
			pos += len;
			return len;
		}
		
		size_t getSize() const { return bufSize; }
		
	private:
		size_t pos;
		size_t const bufSize;
		uint8_t* const buf;
};

template<const bool managed>
class BufferedInputStream : public InputStream
{
	public:
		BufferedInputStream(InputStream* stream, size_t bufSize) : s(stream), pos(0), size(0), bufSize(bufSize), buf(new uint8_t[bufSize])
		{
		}
		~BufferedInputStream()
		{
			if (managed) delete s;
			delete[] buf;
		}
		
		size_t read(void* out, size_t& len) override
		{
			size_t sizeInBuf = size - pos;
			if (len > sizeInBuf)
			{
				if (pos)
				{
					size -= pos;
					memmove(buf, buf + pos, size);
					pos = 0;
				}
				size_t inSize = bufSize - size;
				if (inSize)
				{
					size += s->read(buf + size, inSize);
					sizeInBuf = size - pos;
				}
				if (len > sizeInBuf) len = sizeInBuf;
			}
			memcpy(out, buf + pos, len);
			pos += len;
			return len;
		}

		size_t rewind(size_t delta)
		{
			if (delta > pos) delta = pos;
			pos -= delta;
			return delta;
		}

		void closeStream() override
		{
			s->closeStream();
		}

	private:
		InputStream* const s;
		size_t const bufSize;
		size_t pos;
		size_t size;
		uint8_t* const buf;
};

class IOStream : public InputStream, public OutputStream
{
};

template<const bool managed>
class LimitedInputStream : public InputStream
{
	public:
		explicit LimitedInputStream(InputStream* is, int64_t maxBytes) : s(is), maxBytes(maxBytes) {}

		~LimitedInputStream()
		{
			if (managed) delete s;
		}
		
		size_t read(void* buf, size_t& len) override
		{
			dcassert(maxBytes >= 0);
			len = (size_t)min(maxBytes, (int64_t)len);
			if (len == 0)
				return 0;
			size_t x = s->read(buf, len);
			maxBytes -= x;
			return x;
		}
		
		void closeStream() override
		{
			s->closeStream();
		}
		
	private:
		InputStream* const s;
		int64_t maxBytes;
};

/** Limits the number of bytes that are requested to be written (not the number actually written!) */
class LimitedOutputStream : public OutputStream
{
	public:
		explicit LimitedOutputStream(OutputStream* os, uint64_t maxBytes) : s(os), maxBytes(maxBytes)
		{
		}
		~LimitedOutputStream()
		{
			delete s;
		}
		
		size_t write(const void* buf, size_t len) override
		{
			//dcassert(len > 0);
			if (maxBytes < len)
			{
#ifndef NO_RESOURCE_MANAGER
				throw FileException(STRING(TOO_MUCH_DATA));
#else
				throw FileException("More data was sent than was expected");
#endif
			}
			maxBytes -= len;
			return s->write(buf, len);
		}
		
		size_t flushBuffers(bool force) override
		{
			return s->flushBuffers(force);
		}
		
		virtual bool eof() const
		{
			return maxBytes == 0;
		}
		
	private:
		OutputStream* const s;
		uint64_t maxBytes;
};

template<const bool managed>
class BufferedOutputStream : public OutputStream
{
	public:
		using OutputStream::write;
		
		explicit BufferedOutputStream(OutputStream* stream, size_t bufSize) : s(stream),
			pos(0), bufSize(bufSize), buf(bufSize ? new uint8_t[bufSize] : nullptr)
		{
		}

		~BufferedOutputStream()
		{
			try
			{
				// We must do this in order not to lose bytes when a download
				// is disconnected prematurely
				flushBuffers(true);
			}
			catch (const Exception&)
			{
			}
			if (managed) delete s;
			delete[] buf;
		}
		
		size_t flushBuffers(bool force) override
		{
			if (pos > 0)
			{
				s->write(buf, pos);
				pos = 0;
			}
			s->flushBuffers(force);
			return 0;
		}
		
		size_t write(const void* wbuf, size_t len) override
		{
			const uint8_t* b = static_cast<const uint8_t*>(wbuf);
			size_t result = len;
			while (len > 0)
			{
				if (pos == 0 && len >= bufSize)
				{
					s->write(b, len);
					break;
				}
				size_t n = min(bufSize - pos, len);
				memcpy(buf + pos, b, n);
				b += n;
				pos += n;
				len -= n;
				if (pos == bufSize)
				{
					s->write(buf, bufSize);
					pos = 0;
				}
			}
			return result;
		}

	private:
		OutputStream* const s;
		size_t pos;
		size_t const bufSize;
		uint8_t* const buf;
};

class StringOutputStream : public OutputStream
{
	public:
		explicit StringOutputStream(string& out) : str(out) { }
		using OutputStream::write;
		
		size_t flushBuffers(bool force) override
		{
			return 0;
		}

		size_t write(const void* buf, size_t len) override
		{
			str.append(static_cast<const char*>(buf), len);
			return len;
		}

		size_t getOutputSize() const { return str.length(); }

	private:
		string& str;
};

#endif // !defined(STREAMS_H)

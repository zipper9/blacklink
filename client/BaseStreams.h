#ifndef BASE_STREAMS_H_
#define BASE_STREAMS_H_

#include "Exception.h"

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

class IOStream : public InputStream, public OutputStream
{
};

#endif // BASE_STREAMS_H_

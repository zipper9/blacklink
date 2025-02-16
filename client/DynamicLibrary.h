#ifndef DYNAMIC_LIBRARY_H_
#define DYNAMIC_LIBRARY_H_

#include "tstring.h"

#ifdef _WIN32
#include "w.h"
#endif

class DynamicLibrary
{
	public:
		DynamicLibrary() : lib(nullptr) {}
		~DynamicLibrary() { close(); }

		DynamicLibrary(const DynamicLibrary&) = delete;
		DynamicLibrary& operator= (const DynamicLibrary &) = delete;

		bool open(const tstring& path) noexcept;
		bool open(const tchar_t* path) noexcept;
		void close() noexcept;
		void* resolve(const char* name) noexcept;
		bool isOpen() const { return lib != nullptr; }

	protected:
#ifdef _WIN32
		HMODULE lib;
#else
		void* lib;
#endif
};

#endif // DYNAMIC_LIBRARY_H_

#include "stdinc.h"

#include "DynamicLibrary.h"
#include "Text.h"
#include "debug.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

bool DynamicLibrary::open(const tstring& path) noexcept
{
	if (lib) return true;
#ifdef _WIN32
	lib = LoadLibrary(path.c_str());
#else
	lib = dlopen(Text::fromT(path).c_str(), RTLD_NOW);
#endif
	return lib != nullptr;
}

bool DynamicLibrary::open(const tchar_t* path) noexcept
{
	if (lib) return true;
#ifdef _WIN32
	lib = LoadLibrary(path);
#else
#ifdef _UNICODE
	string s;
	Text::wideToUtf8(path, wcslen(path), s);
	lib = dlopen(s.c_str(), RTLD_NOW);
#else
	lib = dlopen(path, RTLD_NOW);
#endif
#endif
	return lib != nullptr;
}

void DynamicLibrary::close() noexcept
{
	if (lib)
	{
#ifdef _WIN32
		FreeLibrary(lib);
#else
		dlclose(lib);
#endif
		lib = nullptr;
	}
}

void* DynamicLibrary::resolve(const char* name) noexcept
{
	dcassert(lib);
#ifdef _WIN32
	return GetProcAddress(lib, name);
#else
	return dlsym(lib, name);
#endif
}

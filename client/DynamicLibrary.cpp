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

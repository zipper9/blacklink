#ifndef PATH_UTIL_H_
#define PATH_UTIL_H_

#include "Path.h"
#include "Text.h"
#include <algorithm>

namespace Util
{
	template<typename string_t>
	static string_t getFilePath(const string_t& path)
	{
		const auto i = path.rfind(PATH_SEPARATOR);
		return i != string_t::npos ? path.substr(0, i + 1) : path;
	}

	template<typename string_t>
	inline string_t getFileName(const string_t& path)
	{
		const auto i = path.rfind(PATH_SEPARATOR);
		return i != string_t::npos ? path.substr(i + 1) : path;
	}

	template<typename string_t>
	inline string_t getFileExtWithoutDot(const string_t& path)
	{
		const auto i = path.rfind('.');
		if (i == string_t::npos) return string_t();
		const auto j = path.rfind(PATH_SEPARATOR);
		if (j != string_t::npos && j > i) return string_t();
		return path.substr(i + 1);
	}

	template<typename string_t>
	inline string_t getFileExt(const string_t& path)
	{
		const auto i = path.rfind('.');
		if (i == string_t::npos) return string_t();
		const auto j = path.rfind(PATH_SEPARATOR);
		if (j != string_t::npos && j > i) return string_t();
		return path.substr(i);
	}

	template<typename string_t>
	inline string_t getLastDir(const string_t& path)
	{
		const auto i = path.rfind(PATH_SEPARATOR);
		if (i == string_t::npos) return string_t();
		const auto j = path.rfind(PATH_SEPARATOR, i - 1);
		return j != string_t::npos ? path.substr(j + 1, i - j - 1) : path;
	}

	template<typename string_type>
	inline bool checkFileExt(const string_type& filename, const string_type& ext)
	{
		if (filename.length() <= ext.length()) return false;
		return Text::isAsciiSuffix2<string_type>(filename, ext);
	}

	template <class T>
	inline void appendPathSeparator(T& path)
	{
		if (!path.empty() && path.back() != PATH_SEPARATOR && path.back() != '/')
			path += typename T::value_type(PATH_SEPARATOR);
	}

	template<typename T>
	inline void removePathSeparator(T& path)
	{
#ifdef _WIN32
		if (path.length() == 3 && path[1] == ':') return; // Drive letter, like "C:\"
#endif
		if (!path.empty() && path.back() == PATH_SEPARATOR)
			path.erase(path.length()-1);
	}

	template<typename T>
	inline void uriSeparatorsToPathSeparators(T& str)
	{
#ifdef _WIN32
		std::replace(str.begin(), str.end(), '/', PATH_SEPARATOR);
#endif
	}

	template<typename T>
	inline void toNativePathSeparators(T& path)
	{
#ifdef _WIN32
		std::replace(path.begin(), path.end(), '/', '\\');
#else
		std::replace(path.begin(), path.end(), '\\', '/');
#endif
	}

	template<typename T>
	inline bool isReservedDirName(const T& name)
	{
		if (name.empty() || name.length() > 2) return false;
		if (name[0] != '.') return false;
		return name.length() == 1 || name[1] == '.';
	}
}

#endif // PATH_UTIL_H_

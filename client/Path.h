#ifndef PATH_H_
#define PATH_H_

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#define FULL_MAX_PATH 32768
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#endif

#endif // PATH_H_

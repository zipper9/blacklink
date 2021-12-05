#ifndef SOCK_DEFS_H_
#define SOCK_DEFS_H_

#ifdef _WIN32

#include <winsock2.h>

typedef int socklen_t;
typedef SOCKET socket_t;
#define SE_EWOULDBLOCK WSAEWOULDBLOCK
#define SE_EADDRINUSE  WSAEADDRINUSE

#else

#include <sys/socket.h>

typedef int socket_t;
static const int INVALID_SOCKET = -1;
#define SOCKET_ERROR -1
#define SE_EWOULDBLOCK EWOULDBLOCK
#define SE_EADDRINUSE  EADDRINUSE

#endif

#endif // SOCK_DEFS_H_

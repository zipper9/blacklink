#ifndef SOCK_DEFS_H_
#define SOCK_DEFS_H_

#ifdef _WIN32
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#include <winsock2.h>
#else
#include <ws2def.h>
#endif
#else
#include <sys/socket.h>
#endif

#endif // SOCK_DEFS_H_

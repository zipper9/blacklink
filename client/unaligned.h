#ifndef UNALIGNED_H_
#define UNALIGNED_H_

#include <stdint.h>

#ifdef _MSC_VER

#if defined _M_ARM || defined _M_ARM64
#define _UNALIGNED_ __unaligned
#else
#define _UNALIGNED_
#endif

#define DECLARE_UNALIGNED(b) \
static __forceinline uint ## b ## _t loadUnaligned ## b(const void* mem) \
{ return *(const _UNALIGNED_ uint ## b ## _t*) mem; } \
static __forceinline void storeUnaligned ## b(void* mem, uint ## b ## _t val) \
{ *(_UNALIGNED_ uint ## b ## _t*) mem = val; }

#elif defined(__GNUC__)

#define DECLARE_UNALIGNED(b) \
struct _unaligned_ ## b ## _s { uint ## b ## _t x; } __attribute__((packed)); \
static inline __attribute__((__always_inline__)) uint ## b ## _t loadUnaligned ## b(const void* mem) \
{ return ((const struct _unaligned_ ## b ## _s*) mem)->x; } \
static inline __attribute__((__always_inline__)) void storeUnaligned ## b(void* mem, uint ## b ## _t val) \
{ ((struct _unaligned_ ## b ## _s*) mem)->x = val; }

#else

#include <string.h>

#define DECLARE_UNALIGNED(b) \
static uint ## b ## _t loadUnaligned ## b(const void* mem) \
{ uint ## b ## _t val; memcpy(&val, mem, sizeof(val)); return val; } \
static void storeUnaligned ## b(void* mem, uint ## b ## _t val) \
{ memcpy(mem, &val, sizeof(val)); }

#endif

DECLARE_UNALIGNED(16)
DECLARE_UNALIGNED(32)
DECLARE_UNALIGNED(64)

#endif

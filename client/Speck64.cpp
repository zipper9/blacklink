#include "stdinc.h"
#include "Speck64.h"
#include "unaligned.h"

static inline uint32_t ROTL32(uint32_t x, int r)
{
	return (x << r) | (x >> (32 - r));
}

#define ER32(x,y,k) \
	do { x = ROTL32(x, 24); x += y; x ^= k; y = ROTL32(y, 3); y ^= x; } while (0)

#define DR32(x,y,k) \
	do { y ^= x; y = ROTL32(y, 29); x ^= k; x -= y; x = ROTL32(x, 8); } while (0)

void Speck64::setKey(const void* key)
{
	const uint8_t* pk = static_cast<const uint8_t*>(key);
	uint32_t a = loadUnaligned32(pk);
	uint32_t b = loadUnaligned32(pk + 4);
	uint32_t c = loadUnaligned32(pk + 8);
	uint32_t d = loadUnaligned32(pk + 12);
	for (int i = 0; i < ROUNDS;)
	{
		rk[i] = a; ER32(b, a, i++);
		rk[i] = a; ER32(c, a, i++);
		rk[i] = a; ER32(d, a, i++);
	}
}

void Speck64::encryptBlock(uint32_t data[]) const
{
	uint32_t x = data[0];
	uint32_t y = data[1];
	for (int i = 0; i < ROUNDS; i++) ER32(y, x, rk[i]);
	data[0] = x;
	data[1] = y;
}

void Speck64::decryptBlock(uint32_t data[]) const
{
	uint32_t x = data[0];
	uint32_t y = data[1];
	for (int i = ROUNDS-1; i >= 0; i--) DR32(y, x, rk[i]);
	data[0] = x;
	data[1] = y;
}

void Speck64::encryptBlock(const void* in, void* out) const
{
	uint32_t data[2];
	data[0] = loadUnaligned32(in);
	data[1] = loadUnaligned32(static_cast<const uint8_t*>(in) + 4);
	encryptBlock(data);
	storeUnaligned32(out, data[0]);
	storeUnaligned32(static_cast<uint8_t*>(out) + 4, data[1]);
}

void Speck64::decryptBlock(const void* in, void* out) const
{
	uint32_t data[2];
	data[0] = loadUnaligned32(in);
	data[1] = loadUnaligned32(static_cast<const uint8_t*>(in) + 4);
	decryptBlock(data);
	storeUnaligned32(out, data[0]);
	storeUnaligned32(static_cast<uint8_t*>(out) + 4, data[1]);
}

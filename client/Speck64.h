#ifndef SPECK64_H_
#define SPECK64_H_

#include <stdint.h>

class Speck64
{
public:
	void setKey(const void* key); // 16 bytes
	void encryptBlock(uint32_t data[]) const; // 2 uint32_t words = 8 bytes
	void decryptBlock(uint32_t data[]) const;
	void encryptBlock(const void* in, void* out) const;
	void decryptBlock(const void* in, void* out) const;

private:
	static const int ROUNDS = 27;
	uint32_t rk[ROUNDS];
};

#endif // SPECK64_H_

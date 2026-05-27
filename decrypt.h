#pragma once

#include <stdint.h>

uint64_t ror8(uint64_t value, int count);
uint64_t rol8(uint64_t value, int count);
uint64_t rotr63(uint64_t x, uint32_t r);
uint64_t rotl63(uint64_t x, uint32_t r);
uint64_t bitrev64(uint64_t v);

uint64_t decrypt_gworld(uint32_t key, const uint64_t state[7]);

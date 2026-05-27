//github.com/ege64
//git.com/ege64
//gitclone.com/ege64


#include "decrypt.h"

uint64_t ror8(uint64_t value, int count) {
    const int nbits = 64;
    count = count % nbits;
    if (count < 0) count += nbits;
    return (value >> count) | (value << (nbits - count));
}

uint64_t rol8(uint64_t value, int count) {
    const int nbits = 64;
    count = count % nbits;
    if (count < 0) count += nbits;
    return (value << count) | (value >> (nbits - count));
}

uint64_t rotr63(uint64_t x, uint32_t r) {
    uint8_t s = (uint8_t)(r % 0x3F) + 1;
    return (x >> s) | (x << (64 - s));
}

uint64_t rotl63(uint64_t x, uint32_t r) {
    uint8_t s = (uint8_t)(r % 0x3F) + 1;
    return (x << s) | (x >> (64 - s));
}

uint64_t bitrev64(uint64_t v) {
    uint64_t t  = (v >> 1) ^ ((v >> 1) ^ (2 * v)) & 0xAAAAAAAAAAAAAAAAULL;
    uint64_t t2 = (t >> 2) ^ ((t >> 2) ^ (4 * t)) & 0xCCCCCCCCCCCCCCCCULL;
    uint64_t t3 = (t2 >> 4) ^ ((t2 >> 4) ^ (16 * t2)) & 0xF0F0F0F0F0F0F0F0ULL;
    return ror8((t3 >> 8) ^ ((t3 >> 8) ^ (t3 << 8)) & 0xFF00FF00FF00FF00ULL, 32);
}

uint64_t decrypt_gworld(uint32_t key, const uint64_t state[7]) {
    uint32_t k = key;

    uint64_t v26 = 0x2545F4914F6CDD1DULL *
        (uint64_t)((uint32_t)k ^
            (uint32_t)(((uint32_t)k ^ (uint32_t)((uint64_t)(uint32_t)k >> 15)) >> 12) ^
            (uint32_t)(k << 25));

    uint64_t v27 = v26 % 7;
    uint64_t v28 = state[v27];
    uint32_t v29 = (uint32_t)(v26 >> 32);
    uint32_t v30 = (uint32_t)(v27 % 7);
    uint64_t v34 = 0xAAAAAAAAAAAAAAAAULL;

    if ((uint32_t)v27 == 7 * ((uint32_t)v27 / 7)) {
        v34 = bitrev64(~v28);
        goto done;
    }

    if (v30 == 1) {
        uint32_t v45 = v29 + 2 * (uint32_t)v27;
        v34 = (uint64_t)(uint32_t)(v29 + (uint32_t)v27) + rotr63(v28, v45);
        goto fallthrough;
    }

    if (v30 == 2) {
        uint32_t r = (uint32_t)(v29 + (uint32_t)v27);
        v34 = rotl63(~v28, r);
        goto done;
    }

fallthrough:
    v34 = v28;

    switch ((uint32_t)v27 % 7) {
    case 3: {
        uint64_t v46 = ~(uint64_t)(uint32_t)(v29 + 2 * (uint32_t)v27) ^ v28;
        v34 = (v46 >> 1) ^ ((v46 >> 1) ^ (2 * v46)) & 0xAAAAAAAAAAAAAAAAULL;
        break;
    }
    case 4:
        v34 = ~bitrev64(v28);
        break;
    case 5: {
        uint32_t r = (uint32_t)(v29 + (uint32_t)v27);
        v34 = rotl63(~v28, r);
        break;
    }
    case 6:
        v34 = bitrev64(v28);
        break;
    default:
        break;
    }

done:
    return v34 ^ (uint64_t)key;
}

//fuck 12.10 patch
namespace decryptuworld {
typedef __int64 ll;
typedef unsigned __int64 ull;
typedef char int8;
typedef signed char sint8;
typedef unsigned char uint8;
typedef short int16;
typedef signed short sint16;
typedef unsigned short uint16;
typedef int int32;
typedef signed int sint32;
typedef unsigned int uint32;
typedef ll int64;
typedef ll sint64;
typedef ull uint64;
typedef unsigned int uint;

template<class T> T __ROL__(T value, int count)
{
const uint nbits = sizeof(T) * 8;
if (count > 0)
{
count %= nbits;
T high = value >> (nbits - count);
if (T(-1) < 0)
high &= ~((T(-1) << count));
value <<= count;
value |= high;
}
else
{
count = -count % nbits;
T low = value << (nbits - count);
value >>= count;
value |= low;
}
return value;
}

inline uint8 __ROL1__(uint8 value, int count) { return __ROL__((uint8)value, count); }
inline uint16 __ROL2__(uint16 value, int count) { return __ROL__((uint16)value, count); }
inline uint32 __ROL4__(uint32 value, int count) { return __ROL__((uint32)value, count); }
inline uint64 __ROL8__(uint64 value, int count) { return __ROL__((uint64)value, count); }
inline uint8 __ROR1__(uint8 value, int count) { return __ROL__((uint8)value, -count); }
inline uint16 __ROR2__(uint16 value, int count) { return __ROL__((uint16)value, -count); }
inline uint32 __ROR4__(uint32 value, int count) { return __ROL__((uint32)value, -count); }
inline uint64 __ROR8__(uint64 value, int count) { return __ROL__((uint64)value, -count); }

struct State {
uintptr_t keys[7];
};

inline uint64_t rotr63(uint64_t x, uint32_t r) {
uint8_t s = (uint8_t)(r % 0x3F) + 1;
return (x >> s) | (x << (64 - s));
}

inline uint64_t rotl63(uint64_t x, uint32_t r) {
uint8_t s = (uint8_t)(r % 0x3F) + 1;
return (x << s) | (x >> (64 - s));
}

inline uint64_t bitrev64(uint64_t v) {
uint64_t t = (v >> 1) ^ ((v >> 1) ^ (2 * v)) & 0xAAAAAAAAAAAAAAAAULL;
uint64_t t2 = (t >> 2) ^ ((t >> 2) ^ (4 * t)) & 0xCCCCCCCCCCCCCCCCULL;
uint64_t t3 = (t2 >> 4) ^ ((t2 >> 4) ^ (16 * t2)) & 0xF0F0F0F0F0F0F0F0ULL;
return __ROR8__((t3 >> 8) ^ ((t3 >> 8) ^ (t3 << 8)) & 0xFF00FF00FF00FF00ULL, 32);
}

uint64_t decrypt_gworld_b1(uint32_t key, const uint64_t* state) {
uint32_t k = key;

uint64_t v26 = 0x2545F4914F6CDD1DULL *
(uint64_t)((uint32_t)k ^
(uint32_t)(((uint32_t)k ^ (uint32_t)((uint64_t)(uint32_t)k >> 15)) >> 12) ^
(uint32_t)(k << 25));

uint64_t v27 = v26 % 7;
uint64_t v28 = state[v27];
uint32_t v29 = (uint32_t)(v26 >> 32); // HIDWORD
uint32_t v30 = (uint32_t)(v27 % 7);
uint64_t v34 = 0xAAAAAAAAAAAAAAAAULL;

if ((uint32_t)v27 == 7 * ((uint32_t)v27 / 7)) {
v34 = bitrev64(~v28);
goto LABEL_32;
}

if (v30 == 1) {
uint32_t v45 = v29 + 2 * (uint32_t)v27;
v34 = (uint64_t)(uint32_t)(v29 + (uint32_t)v27)
+ rotl63(v28,
63 * (uint8_t)(v45 / 0x3F)
- ((uint8_t)v29 + 2 * (uint8_t)(uint32_t)v27)
+ 63);
v34 = (uint64_t)(uint32_t)(v29 + (uint32_t)v27) + rotr63(v28, v45);
goto LABEL_39;
}

if (v30 == 2) {
uint32_t r = (uint32_t)(v29 + (uint32_t)v27);
v34 = rotl63(~v28, r);
goto LABEL_32;
}

LABEL_39:
v34 = v28;

switch ((uint32_t)v27 % 7) {
case 3: {
uint64_t v46 = ~(uint64_t)(uint32_t)(v29 + 2 * (uint32_t)v27) ^ v28;
v34 = (v46 >> 1) ^ ((v46 >> 1) ^ (2 * v46)) & 0xAAAAAAAAAAAAAAAAULL;
break;
}
case 4: {
v34 = ~bitrev64(v28);
break;
}
case 5: {
uint32_t r = (uint32_t)(uint32_t)(v29 + (uint32_t)v27);
v34 = rotl63(~v28, r);
break;
}
case 6: {
v34 = bitrev64(v28);
break;
}
default:
break;
}

LABEL_32:
uint64_t v35 = v34 ^ (uint64_t)(uint32_t)key;
return v35;
}

uintptr_t decryptWorld() {
uintptr_t GWORLD_STATE = 0xA7C38C0;
uintptr_t GWORLD_KEY = GWORLD_STATE + 0x38;

printf(xorstr("----START DECRYPT GWORLD (NEW)----\n"));

uint32_t key = data::read_universal<uint32_t>(gamein.Base + GWORLD_KEY);
printf(xorstr("Key: [0x%x]\n"), key);

State state = data::read_universal<State>(gamein.Base + GWORLD_STATE);
for (int i = 0; i < 7; i++)
printf(xorstr("State[%d]: [0x%llx]\n"), i, state.keys[i]);

uint64_t ptr_addr = decrypt_gworld_b1(key, state.keys);
printf(xorstr("Ptr address: [0x%llx]\n"), ptr_addr);

if (ptr_addr < 0x10000000000ULL || ptr_addr > 0x7FFFFFFFFFFFULL) {
printf(xorstr("[-] BAD ptr_addr: 0x%llx\n"), ptr_addr);
return 0;
}

uintptr_t uworld = data::read_universal<uintptr_t>(ptr_addr);
printf(xorstr("UWorld: [0x%llx]\n"), uworld);

if (uworld < 0x10000000000ULL || uworld > 0x7FFFFFFFFFFFULL) {
printf(xorstr("[-] BAD UWorld\n"));
while (true) {
uworld = data::read_universal<uintptr_t>(ptr_addr);
if (uworld) {
printf(xorstr("UWorld: [0x%llx]\n"), uworld);
return uworld;
}
}
return 0;
}

printf(xorstr("----END DECRYPT----\n\n"));
return uworld;
}
}

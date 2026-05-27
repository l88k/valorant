/*
 * valread.c
 *
 * standalone ue5 memory reader for VALORANT-Win64-Shipping.exe.
 * reads gnames, gobjects, actors, class properties and entity data
 * via ReadProcessMemory — no injection or driver required.
 *
 * build:  cl /nologo /O2 valread.c /Fe:valread.exe advapi32.lib
 * run:    valread.exe <command>  (run as administrator)
 *
 * note: Riot Vanguard (vgk) blocks RPM in live matches.
 *       run during lobby or training range without vgk loaded.
 *
 * offsets: patch 12.09  — update GWorld when the game patches.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define GWorld        0xA7C38C0ULL
#define FNamePool     0xA547C00ULL
#define FNameState    0xA57ACC0ULL
#define GObjects      0xEE2700ULL

#define GameState     0x178
#define PlayerArray   0x480
#define PawnPrivate   0x4E8
#define RootComponent 0x290
#define RelativeLoc   0x170
#define DamageHandler 0xC68
#define Health        0x200
#define TeamComponent 0x6A8
#define TeamId        0xE8

#define FNameChunkSize  0x4000
#define FNameMaxChunks  512

static HANDLE    hProc  = NULL;
static uint64_t  base   = 0;
static uint32_t  xorKey = 0;

/* process helpers */

static int enable_debug_priv(void) {
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
        return 0;
    TOKEN_PRIVILEGES tp = { .PrivilegeCount = 1 };
    LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL ok = AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), NULL, NULL);
    CloseHandle(tok);
    return ok && GetLastError() == ERROR_SUCCESS;
}

static DWORD find_pid(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { .dwSize = sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static uint64_t find_module_base(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me = { .dwSize = sizeof(me) };
    uint64_t base = 0;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, name) == 0) { base = (uint64_t)me.modBaseAddr; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

static int rvm(uint64_t addr, void* out, size_t sz) {
    if (!addr) return 0;
    SIZE_T rd = 0;
    return ReadProcessMemory(hProc, (LPCVOID)addr, out, sz, &rd) && rd > 0;
}

static uint64_t rptr(uint64_t addr) { uint64_t v = 0; rvm(addr, &v, 8); return v; }
static int32_t  ri32(uint64_t addr) { int32_t  v = 0; rvm(addr, &v, 4); return v; }

typedef struct { float x, y, z; } vec3;
static vec3 rvec3(uint64_t addr) { vec3 v = {0,0,0}; rvm(addr, &v, sizeof(v)); return v; }

/* crypto helpers (must match forum's IDA decompilation exactly) */

static uint64_t swap_pairs(uint64_t x) {
    return (x >> 1) ^ (((x >> 1) ^ (x << 1)) & 0xAAAAAAAAAAAAAAAAULL);
}
static uint64_t bitrev64(uint64_t v) {
    v = (v >> 1) ^ (((v >> 1) ^ (v << 1)) & 0xAAAAAAAAAAAAAAAAULL);
    v = (v >> 2) ^ (((v >> 2) ^ (v << 2)) & 0xCCCCCCCCCCCCCCCCULL);
    v = (v >> 4) ^ (((v >> 4) ^ (v << 4)) & 0xF0F0F0F0F0F0F0F0ULL);
    v = (v >> 8) ^ (((v >> 8) ^ (v << 8)) & 0xFF00FF00FF00FF00ULL);
    return (v >> 32) | (v << 32);
}
static uint64_t rotl63(uint64_t x, uint32_t r) {
    uint8_t s = (uint8_t)(r % 0x3F) + 1;
    return (x << s) | (x >> (64 - s));
}
static uint64_t rotr63(uint64_t x, uint32_t r) {
    uint8_t s = (uint8_t)(r % 0x3F) + 1;
    return (x >> s) | (x << (64 - s));
}

static uint64_t decrypt_state(uint32_t key, const uint64_t* state) {
    uint64_t v26 = 0x2545F4914F6CDD1DULL *
        (uint64_t)((uint32_t)key ^
            (uint32_t)(((uint32_t)key ^ (uint32_t)((uint64_t)(uint32_t)key >> 15)) >> 12) ^
            (uint32_t)(key << 25));

    uint32_t v27 = (uint32_t)(v26 % 7);
    uint64_t v28 = state[v27];
    uint32_t v29 = (uint32_t)(v26 >> 32);
    uint64_t v34;

    switch (v27) {
    case 0: v34 = bitrev64(~v28);                                          break;
    case 1: v34 = v28;                                                     break;
    case 2: v34 = rotl63(~v28, v29 + v27);                                 break;
    case 3: v34 = swap_pairs(~(uint64_t)(uint32_t)(v29 + 2*v27) ^ v28);    break;
    case 4: v34 = ~bitrev64(v28);                                          break;
    case 5: v34 = rotl63(~v28, v29 + v27);                                 break;
    case 6: v34 = bitrev64(v28);                                           break;
    default: v34 = v28;                                                    break;
    }
    return v34 ^ (uint64_t)(uint32_t)key;
}

static uint64_t try_decrypt_op(int op, uint64_t v28, uint32_t v29, uint32_t v27, uint32_t key) {
    uint64_t v34;
    switch (op) {
    case 0: v34 = bitrev64(~v28);                                          break;
    case 1: v34 = v28;                                                     break;
    case 2: v34 = rotl63(~v28, v29 + v27);                                 break;
    case 3: v34 = swap_pairs(~(uint64_t)(uint32_t)(v29 + 2*v27) ^ v28);    break;
    case 4: v34 = ~bitrev64(v28);                                          break;
    case 5: v34 = rotl63(~v28, v29 + v27);                                 break;
    case 6: v34 = bitrev64(v28);                                           break;
    case 7: v34 = (uint64_t)(uint32_t)(v29 + v27) + rotr63(v28, v29 + 2*v27); break;
    case 8: v34 = rotr63(v28, v29 + v27);                                  break;
    case 9: v34 = swap_pairs(v28);                                         break;
    case 10: v34 = rotl63(v28, v29 + v27);                                 break;
    case 11: v34 = rotr63(~v28, v29 + v27);                                break;
    case 12: v34 = ~v28;                                                   break;
    default: v34 = v28;                                                    break;
    }
    return v34 ^ (uint64_t)(uint32_t)key;
}

static uint64_t get_uworld(void) {
    uint64_t sa = base + GWorld;

    /* retry loop — state is dynamic, read atomically and retry */
    for (int attempt = 0; attempt < 10; attempt++) {
        uint8_t raw[0x40] = {0};
        rvm(sa, raw, 0x3C); /* read 7 * uint64 + uint32 key in one call */
        uint64_t state[7];
        memcpy(state, raw, 56);
        uint32_t key;
        memcpy(&key, raw + 56, 4);

        /* standard decrypt — hash the key, pick state slot and op */
        uint64_t ptr_addr = decrypt_state(key, state);
        if (ptr_addr > 0x10000000000ULL && ptr_addr < 0x7FFFFFFFFFFFULL) {
            uint64_t uw = rptr(ptr_addr);
            if (uw > 0x10000000000ULL && uw < 0x7FFFFFFFFFFFULL) {
                uint64_t vt = rptr(uw);
                if (vt >= base && vt < base + 0x10000000ULL)
                    return uw;
            }
        }

        /* fallback: brute-force all 13 ops × 7 state indices */
        for (int si = 0; si < 7; si++) {
            uint64_t v26 = 0x2545F4914F6CDD1DULL *
                (uint64_t)((uint32_t)key ^
                    (uint32_t)(((uint32_t)key ^ (uint32_t)((uint64_t)(uint32_t)key >> 15)) >> 12) ^
                    (uint32_t)(key << 25));
            uint32_t v29 = (uint32_t)(v26 >> 32);
            for (int op = 0; op < 13; op++) {
                uint64_t pa = try_decrypt_op(op, state[si], v29, (uint32_t)si, key);
                if (pa < 0x10000000000ULL || pa > 0x7FFFFFFFFFFFULL) continue;
                uint64_t uw = rptr(pa);
                if (uw < 0x10000000000ULL || uw > 0x7FFFFFFFFFFFULL) continue;
                uint64_t vt = rptr(uw);
                if (vt < base || vt >= base + 0x10000000ULL) continue;
                return uw;
            }
        }

        Sleep(1);
    }

    return 0;
}

/* fname pool scanner — two methods: lea pattern in .data, then raw heap scan */

static int is_heap_ptr(uint64_t v) {
    return v > 0x10000ULL && v < 0x800000000000ULL && (v & 0x7) == 0;
}

static uint64_t scan_fname_pool_lea(uint64_t text_va, uint32_t text_sz,
                                     uint64_t data_va, uint32_t data_sz) {
    (void)text_va; (void)text_sz;

    typedef struct { uint64_t base; uint64_t size; } region_t;
    region_t regions[256];
    int nregions = 0;
    {
        uint64_t a = 0;
        while (a < 0x800000000000ULL && nregions < 256) {
            MEMORY_BASIC_INFORMATION mbi = {0};
            if (!VirtualQueryEx(hProc, (LPCVOID)a, &mbi, sizeof(mbi))) break;
            uint64_t rbase = (uint64_t)mbi.BaseAddress;
            uint64_t rsize = mbi.RegionSize;
            if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE &&
                !(mbi.Protect & (PAGE_GUARD|PAGE_NOACCESS)) && rsize >= 0x100000ULL) {
                if (!(rbase >= base && rbase < base + 0x10000000ULL)) {
                    regions[nregions].base = rbase;
                    regions[nregions].size = rsize;
                    nregions++;
                }
            }
            a = rbase + rsize;
        }
    }
    if (!nregions) return 0;

    const size_t B = 0x20000;
    uint8_t* buf   = (uint8_t*)malloc(B);
    uint8_t* p1buf = (uint8_t*)malloc(0x200);
    uint8_t* chunk = (uint8_t*)malloc(0x8000);
    if (!buf || !p1buf || !chunk) { free(buf); free(p1buf); free(chunk); return 0; }

    uint64_t result = 0;
    for (size_t off = 0; off < data_sz && !result; off += B - 8) {
        size_t rsz = B;
        if (off + rsz > data_sz) rsz = data_sz - off;
        SIZE_T got = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)(data_va + off), buf, rsz, &got) || got < 8)
            continue;
        for (size_t i = 0; i + 8 <= got && !result; i += 8) {
            uint64_t p1 = *(uint64_t*)(buf + i);
            int in_region = 0;
            for (int r = 0; r < nregions; r++) {
                if (p1 >= regions[r].base && p1 < regions[r].base + regions[r].size)
                    { in_region = 1; break; }
            }
            if (!in_region) continue;

            SIZE_T p1got = 0;
            if (!ReadProcessMemory(hProc, (LPCVOID)p1, p1buf, 0x200, &p1got) || p1got < 16)
                continue;
            for (size_t j = 0; j + 8 <= p1got && !result; j += 8) {
                uint64_t p2 = *(uint64_t*)(p1buf + j);
                if (!is_heap_ptr(p2)) continue;
                if (p2 >= base && p2 < base + 0x10000000ULL) continue;
                SIZE_T cgot = 0;
                if (!ReadProcessMemory(hProc, (LPCVOID)p2, chunk, 0x8000, &cgot) || cgot < 8)
                    continue;
                for (size_t k = 0; k + 8 < cgot; k++) {
                    if (chunk[k]!='N'||chunk[k+1]!='o'||chunk[k+2]!='n'||
                        chunk[k+3]!='e'||chunk[k+4]!=0) continue;
                    for (size_t m = k+5; m + 7 < cgot && m < k+4096; m++) {
                        if ((chunk[m]=='O'&&chunk[m+1]=='b'&&chunk[m+2]=='j'&&
                             chunk[m+3]=='e'&&chunk[m+4]=='c'&&chunk[m+5]=='t'&&chunk[m+6]==0)||
                            (chunk[m]=='C'&&chunk[m+1]=='l'&&chunk[m+2]=='a'&&
                             chunk[m+3]=='s'&&chunk[m+4]=='s'&&chunk[m+5]==0)) {
                            result = p1 + j;
                            break;
                        }
                    }
                    if (result) break;
                }
            }
        }
    }
    free(buf); free(p1buf); free(chunk);
    return result;
}

static uint64_t scan_fname_pool(void) {
    uint8_t dos[0x40];
    SIZE_T rd = 0;
    if (!ReadProcessMemory(hProc, (LPCVOID)base, dos, sizeof(dos), &rd)) return 0;
    int32_t pe_off = *(int32_t*)(dos + 0x3C);
    uint8_t pe[0x400];
    if (!ReadProcessMemory(hProc, (LPCVOID)(base + pe_off), pe, sizeof(pe), &rd)) return 0;
    uint16_t nsec  = *(uint16_t*)(pe + 6);
    uint16_t ophsz = *(uint16_t*)(pe + 20);
    uint8_t* secs  = pe + 24 + ophsz;

    uint64_t text_va = 0;
    uint32_t text_sz = 0;
    uint64_t data_va = 0;
    uint32_t data_sz = 0;
    for (int s = 0; s < nsec && s < 64; s++) {
        uint8_t* sec = secs + s * 40;
        uint32_t ch = *(uint32_t*)(sec + 36);
        uint32_t vs = *(uint32_t*)(sec + 16);
        uint32_t vr = *(uint32_t*)(sec + 12);
        if ((ch & 0x20000000) && (ch & 0x40000000) && !(ch & 0x80000000) && vs > text_sz)
            { text_va = base + vr; text_sz = vs; }
        if ((ch & 0x80000000) && (ch & 0x40000000) && !(ch & 0x20000000) && vs > data_sz)
            { data_va = base + vr; data_sz = vs; }
    }

    if (text_va && data_va) {
        uint64_t r = scan_fname_pool_lea(text_va, text_sz, data_va, data_sz);
        if (r) return r;
    }

    printf("    [*] scanning heap for None+ByteProperty...\n");
    const size_t BUFSZ = 0x10000;
    uint8_t* buf = (uint8_t*)malloc(BUFSZ);
    if (!buf) return 0;

    uint64_t none_addr = 0, addr = 0, scanned = 0;
    while (addr < 0x800000000000ULL) {
        MEMORY_BASIC_INFORMATION mbi = {0};
        if (!VirtualQueryEx(hProc, (LPCVOID)addr, &mbi, sizeof(mbi))) break;
        uint64_t rbase = (uint64_t)mbi.BaseAddress;
        uint64_t rsize = mbi.RegionSize;
        if (!(mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD|PAGE_NOACCESS)))) {
            addr = rbase + rsize; continue;
        }
        for (uint64_t boff = 0; boff < rsize; boff += BUFSZ - 32) {
            size_t rsz = BUFSZ;
            if (boff + rsz > rsize) rsz = (size_t)(rsize - boff);
            if (rsz < 32) break;
            SIZE_T got = 0;
            if (!ReadProcessMemory(hProc, (LPCVOID)(rbase + boff), buf, rsz, &got) || got < 32)
                continue;
            scanned += got;
            for (size_t i = 0; i + 30 <= got; i++) {
                if (buf[i]!='N'||buf[i+1]!='o'||buf[i+2]!='n'||buf[i+3]!='e'||buf[i+4]!=0)
                    continue;
                for (size_t g = i+5; g < i+20 && g+13 <= got; g++) {
                    if (buf[g]=='B'&&buf[g+1]=='y'&&buf[g+2]=='t'&&buf[g+3]=='e'&&
                        buf[g+4]=='P'&&buf[g+5]=='r'&&buf[g+6]=='o'&&buf[g+7]=='p'&&
                        buf[g+8]=='e'&&buf[g+9]=='r'&&buf[g+10]=='t'&&buf[g+11]=='y'&&buf[g+12]==0) {
                        none_addr = rbase + boff + i;
                        goto found;
                    }
                }
            }
        }
        addr = rbase + rsize;
    }
found:
    free(buf);
    if (!none_addr) return 0;
    return none_addr | (1ULL << 63);
}

/* FName XOR key: brute force from known "ByteProperty" at index 3 */

static uint32_t brute_force_fname_key(void) {
    uint64_t pool = base + FNamePool;
    uint64_t block0 = rptr(pool + 0x10);
    if (!block0) return 0;
    uint64_t data_ptr = block0 + 4ULL * 3;
    uint16_t header = 0;
    rvm(data_ptr + 4, &header, 2);
    uint16_t len = header >> 1;
    if ((header & 1) || len != 12) return 0;
    uint8_t enc[12];
    rvm(data_ptr + 6, enc, 12);
    const char* plain = "ByteProperty";
    uint32_t key = 0;
    uint8_t* kb = (uint8_t*)&key;
    for (int i = 0; i < 4; i++)
        kb[i] = (uint8_t)(enc[i] ^ (uint8_t)plain[i] ^ (uint8_t)len);
    return key;
}

static void decrypt_fname_str(char* buf, int len, uint32_t key) {
    uint8_t* kb = (uint8_t*)&key;
    for (int i = 0; i < len; i++)
        buf[i] ^= kb[i % 4] ^ (uint8_t)len;
}

/* resolve FName by ComparisonIndex → string */
static int resolve_fname(int32_t index, char* out, int outsz) {
    if (index < 0) { out[0] = 0; return 0; }
    uint32_t block  = (uint32_t)((uint32_t)index >> 16);
    uint16_t offset = (uint16_t)(index & 0xFFFF);
    uint64_t pool = base + FNamePool;
    uint64_t block_ptr = rptr(pool + 0x10 + (uint64_t)block * 8);
    if (!block_ptr) { out[0] = 0; return 0; }
    uint64_t data = block_ptr + (uint64_t)offset * 4;
    uint16_t header = 0;
    rvm(data + 4, &header, 2);
    int wide = header & 1;
    int len  = (header >> 1) & 0x7FFF;
    if (!len || len >= outsz || wide) { out[0] = 0; return 0; }
    rvm(data + 6, out, len);
    out[len] = 0;
    if (xorKey)
        decrypt_fname_str(out, len, xorKey);
    return len;
}

/* init FNamePool: try static offset, brute force XOR key */
static int init_fname_pool(void) {
    uint64_t pool = base + FNamePool;
    uint64_t block0 = rptr(pool + 0x10);
    if (!block0 || block0 > 0x7FFFFFFFFFFFULL) {
        printf("[-] FNamePool block[0] invalid (0x%llx)\n", (unsigned long long)block0);
        return 0;
    }
    xorKey = brute_force_fname_key();
    return 1;
}

/* GNames dump (proper FNamePool with block pointers + XOR decrypt) */

static void cmd_gnames(int limit) {
    if (!init_fname_pool()) {
        printf("[*] falling back to raw scan...\n");
        uint64_t raw = scan_fname_pool();
        if (!raw) { printf("[-] FNamePool not found\n"); return; }
        uint64_t start_addr = raw & ~(3ULL << 62);
        printf("[+] FNames @ 0x%llx (raw scan)\n\n", (unsigned long long)start_addr);
        const size_t RAWSZ = 0x40000;
        uint8_t* blk = (uint8_t*)malloc(RAWSZ);
        if (!blk) return;
        SIZE_T got = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)start_addr, blk, RAWSZ, &got) || got < 8) {
            free(blk); return;
        }
        int    count = 0;
        size_t off   = 0;
        while (off < got && count < limit) {
            while (off < got && blk[off] == 0) off++;
            if (off >= got) break;
            char name[1025];
            int  nlen = 0;
            size_t p = off;
            while (p < got && blk[p] != 0 && nlen < 1024)
                name[nlen++] = (char)blk[p++];
            off = p + 1;
            name[nlen] = 0;
            if (nlen > 0) printf("  [%5d] %s\n", count, name);
            count++;
        }
        free(blk);
        printf("\n[*] %d names\n", count);
        return;
    }

    printf("\n");
    uint64_t pool = base + FNamePool;
    int count = 0;
    const size_t BSREAD = 0x40000;
    uint8_t* bdata = (uint8_t*)malloc(BSREAD);
    if (!bdata) return;

    for (int bi = 0; count < limit; bi++) {
        uint64_t bp = rptr(pool + 0x10 + (uint64_t)bi * 8);
        if (!bp) break;
        SIZE_T bgot = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)bp, bdata, BSREAD, &bgot) || bgot < 8)
            break;
        int off = 0;
        while ((size_t)off * 4 + 6 < bgot && count < limit) {
            size_t pos = (size_t)off * 4;
            uint16_t hdr = *(uint16_t*)(bdata + pos + 4);
            if (!hdr) { off++; continue; }
            int wide = hdr & 1;
            int len = (hdr >> 1) & 0x7FFF;
            if (!len || len > 1024) { off++; continue; }
            int sbytes = wide ? len * 2 : len;
            if (pos + 6 + sbytes > bgot) break;
            if (!wide) {
                char name[1025];
                memcpy(name, bdata + pos + 6, len);
                name[len] = 0;
                if (xorKey)
                    decrypt_fname_str(name, len, xorKey);
                int fid = (bi << 16) | off;
                printf("  [%5d] %s\n", fid, name);
            }
            count++;
            int eslots = (4 + 2 + sbytes + 3) / 4;
            if (eslots < 1) eslots = 1;
            off += eslots;
        }
    }
    free(bdata);
    printf("\n[*] %d names\n", count);
}

/* gobjects dump — scans module for FUObjectArray signature */

#define GObjectsChunkSize  65536
#define FUObjectItemSize   0x18

/* scan .data for FUObjectArray: look for {Objects*, PreAlloc, MaxElems, NumElems, MaxChunks, NumChunks}
 * where Objects is a valid heap ptr, NumElems is 10k-500k, NumChunks is 1-20 */
static uint64_t scan_gobjects(void) {
    uint8_t dos[0x40];
    SIZE_T rd = 0;
    if (!ReadProcessMemory(hProc, (LPCVOID)base, dos, sizeof(dos), &rd)) return 0;
    int32_t pe_off = *(int32_t*)(dos + 0x3C);
    uint8_t pe[0x400];
    if (!ReadProcessMemory(hProc, (LPCVOID)(base + pe_off), pe, sizeof(pe), &rd)) return 0;
    uint16_t nsec  = *(uint16_t*)(pe + 6);
    uint16_t ophsz = *(uint16_t*)(pe + 20);
    uint8_t* secs  = pe + 24 + ophsz;
    uint64_t data_va = 0;
    uint32_t data_sz = 0;
    for (int s = 0; s < nsec && s < 64; s++) {
        uint8_t* sec = secs + s * 40;
        uint32_t ch = *(uint32_t*)(sec + 36);
        uint32_t vs = *(uint32_t*)(sec + 16);
        uint32_t vr = *(uint32_t*)(sec + 12);
        if ((ch & 0x80000000) && (ch & 0x40000000) && !(ch & 0x20000000) && vs > data_sz)
            { data_va = base + vr; data_sz = vs; }
    }
    /* get image size from optional header */
    uint32_t img_size = *(uint32_t*)(pe + 24 + 56);
    if (img_size < 0x1000) img_size = 0x10000000;

    const size_t B = 0x20000;
    uint8_t* buf = (uint8_t*)malloc(B);
    if (!buf) return 0;
    uint64_t result = 0;

    /* scan all committed readable pages in module range */
    uint64_t scan_end = base + img_size;
    uint64_t addr = base;
    while (addr < scan_end && !result) {
        MEMORY_BASIC_INFORMATION mbi = {0};
        if (!VirtualQueryEx(hProc, (LPCVOID)addr, &mbi, sizeof(mbi))) break;
        uint64_t rbase = (uint64_t)mbi.BaseAddress;
        uint64_t rsize = mbi.RegionSize;
        if (rbase + rsize > scan_end) rsize = scan_end - rbase;
        int readable = (mbi.State == MEM_COMMIT) &&
                       (mbi.Protect & (PAGE_READWRITE|PAGE_READONLY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY)) &&
                       !(mbi.Protect & (PAGE_GUARD|PAGE_NOACCESS));
        if (!readable) { addr = rbase + rsize; continue; }
    for (uint64_t boff = 0; boff < rsize && !result; boff += B - 64) {
        size_t rsz = B;
        if (boff + rsz > rsize) rsz = (size_t)(rsize - boff);
        if (rsz < 64) break;
        SIZE_T got = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)(rbase + boff), buf, rsz, &got) || got < 64)
            continue;
        for (size_t i = 0; i + 64 <= got && !result; i += 8) {
            /* try ObjObjects starting at multiple offsets within FUObjectArray */
            static const int obj_offsets[] = {0x00, 0x08, 0x10, 0x18, 0x20};
            for (int li = 0; li < 5 && !result; li++) {
                int obj_off = obj_offsets[li];
                /* FChunkedFixedUObjectArray: Objects(+0), PreAlloc(+8), MaxE(+0x10), NumE(+0x14), MaxC(+0x18), NumC(+0x1C) */
                int ne_off  = obj_off + 0x14;
                int nc_off  = obj_off + 0x1C;
                if (i + nc_off + 4 > got) continue;

                uint64_t objs = *(uint64_t*)(buf + i + obj_off);
                if (!is_heap_ptr(objs)) continue;
                int32_t ne = *(int32_t*)(buf + i + ne_off);
                int32_t nc = *(int32_t*)(buf + i + nc_off);
                if (ne < 1000 || ne > 500000 || nc < 1 || nc > 30) continue;

                uint64_t c0 = rptr(objs);
                if (!c0 || c0 > 0x7FFFFFFFFFFFULL) continue;

                /* try both FUObjectItem sizes */
                static const int item_sizes[] = {0x18, 0x20};
                for (int si = 0; si < 2 && !result; si++) {
                    int isz = item_sizes[si];
                    int valid = 0;
                    char vname[128] = {0};
                    for (int t = 0; t < 10 && t < ne; t++) {
                        uint64_t obj = rptr(c0 + (uint64_t)t * isz);
                        if (!obj || obj > 0x7FFFFFFFFFFFULL) continue;
                        uint64_t vt = rptr(obj);
                        if (vt < base || vt >= base + img_size) continue;
                        uint64_t cls = rptr(obj + 0x10);
                        if (!cls || cls > 0x7FFFFFFFFFFFULL) continue;
                        int32_t nidx = ri32(obj + 0x18);
                        char nm[128] = {0};
                        if (resolve_fname(nidx, nm, sizeof(nm)) <= 0) continue;
                        if (!vname[0]) memcpy(vname, nm, sizeof(vname));
                        valid++;
                    }
                    if (valid < 3) continue;

                    uint64_t faddr = rbase + boff + i;
                    int layout_id = li * 2 + si;
                    printf("[+] gobjects @ 0x%llx (objoff=+0x%x, item=0x%x, %d objs, %d chunks, %d/10 ok)\n",
                           (unsigned long long)faddr, obj_off, isz, ne, nc, valid);
                    printf("    first object name = \"%s\"\n", vname);
                    result = faddr | ((uint64_t)obj_off << 56) | ((uint64_t)isz << 48);
                }
            }
        }
    }
        addr = rbase + rsize;
    }
    free(buf);
    return result;
}

static void cmd_gobjects(int limit) {
    if (!init_fname_pool()) {
        printf("[-] need fnamepool for name resolution\n");
        return;
    }

    /* try static offset first, then scan */
    uint64_t gobjects = base + GObjects;
    int obj_off = 0x10;
    int item_sz = FUObjectItemSize;
    uint64_t obj_array  = rptr(gobjects + obj_off);
    int32_t  num_elems  = ri32(gobjects + obj_off + 0x14);
    int32_t  num_chunks = ri32(gobjects + obj_off + 0x1C);

    if (!obj_array || !is_heap_ptr(obj_array) || num_elems < 1000 || num_chunks < 1 || num_chunks > 256) {
        printf("[!] static gobjects offset invalid, scanning...\n");
        uint64_t scan = scan_gobjects();
        if (!scan) { printf("[-] gobjects not found\n"); return; }
        obj_off  = (int)((scan >> 56) & 0xFF);
        item_sz  = (int)((scan >> 48) & 0xFF);
        gobjects = scan & 0x0000FFFFFFFFFFFFULL;
    }

    obj_array  = rptr(gobjects + obj_off);
    num_elems  = ri32(gobjects + obj_off + 0x14);
    num_chunks = ri32(gobjects + obj_off + 0x1C);
    printf("\n[*] gobjects @ 0x%llx (objoff=+0x%x, item=0x%x)\n",
           (unsigned long long)gobjects, obj_off, item_sz);
    printf("[*] objarray = 0x%llx, NumElements = %d, NumChunks = %d\n",
           (unsigned long long)obj_array, num_elems, num_chunks);

    if (!obj_array || num_elems <= 0 || num_chunks <= 0 || num_chunks > 256) {
        printf("[-] invalid gobjects data\n");
        return;
    }

    uint64_t chunk_ptrs[256] = {0};
    int read_chunks = num_chunks > 256 ? 256 : num_chunks;
    rvm(obj_array, chunk_ptrs, read_chunks * 8);

    int count = 0;
    int total = num_elems < limit ? num_elems : limit;
    printf("\n  %-8s %-18s %-18s %s\n", "Index", "Object", "Class", "Name");

    for (int i = 0; i < total && count < limit; i++) {
        int ci = i / GObjectsChunkSize;
        int si = i % GObjectsChunkSize;
        if (ci >= read_chunks || !chunk_ptrs[ci]) continue;

        uint64_t item_addr = chunk_ptrs[ci] + (uint64_t)si * item_sz;
        uint64_t obj = rptr(item_addr);
        if (!obj) continue;

        int32_t name_idx = ri32(obj + 0x18);
        uint64_t class_ptr = rptr(obj + 0x10);
        char obj_name[256] = {0};
        char cls_name[256] = {0};
        resolve_fname(name_idx, obj_name, sizeof(obj_name));
        if (class_ptr) {
            int32_t cls_name_idx = ri32(class_ptr + 0x18);
            resolve_fname(cls_name_idx, cls_name, sizeof(cls_name));
        }
        if (obj_name[0])
            printf("  %-8d 0x%-16llx %-18s %s\n", i,
                   (unsigned long long)obj, cls_name, obj_name);
        count++;
    }
    printf("\n[*] %d objects\n", count);
}

/* uclass property dumper — auto-detects FField layout then walks property chain */

static int g_cp_off = -1, g_fn_off = -1, g_nx_off = -1, g_po_off = -1;
static int g_ss_off = -1;

static int detect_ffield_layout(uint64_t uclass) {
    static const int cp_try[] = {0x50, 0x48, 0x58, 0x40, 0x60};
    static const int nm_try[] = {0x28, 0x20, 0x18};
    static const int nx_try[] = {0x20, 0x18, 0x10};

    int best_score = 0;
    /* try all combos, pick the one with the most non-zero property offsets
     * (properties have varying offsets, functions all have 0) */
    static const int po_cand[] = {0x44, 0x4C, 0x3C, 0x50, 0x48, 0x40, 0x38};
    int best_po_idx = 0;

    for (int ci = 0; ci < 5; ci++) {
        uint64_t fp = rptr(uclass + cp_try[ci]);
        if (!fp || fp > 0x7FFFFFFFFFFFULL) continue;
        for (int ni = 0; ni < 3; ni++) {
            for (int xi = 0; xi < 3; xi++) {
                if (nx_try[xi] == nm_try[ni]) continue;
                /* walk chain, count entries with names AND try offset candidates */
                for (int poi = 0; poi < 7; poi++) {
                    int chain_len = 0;
                    int32_t seen_offs[64];
                    int n_unique = 0;
                    uint64_t p = fp;
                    while (p && p < 0x7FFFFFFFFFFFULL && chain_len < 100) {
                        int32_t nidx = ri32(p + nm_try[ni]);
                        char nm[128] = {0};
                        if (resolve_fname(nidx, nm, sizeof(nm)) <= 0) break;
                        int32_t off_val = ri32(p + po_cand[poi]);
                        if (off_val >= 0 && off_val < 0x4000 && n_unique < 64) {
                            int dup = 0;
                            for (int u = 0; u < n_unique; u++)
                                if (seen_offs[u] == off_val) { dup = 1; break; }
                            if (!dup) seen_offs[n_unique++] = off_val;
                        }
                        chain_len++;
                        p = rptr(p + nx_try[xi]);
                    }
                    /* score: count of UNIQUE offsets — properties have many different
                     * offsets while functions all share the same constant */
                    int score = n_unique * 1000 + chain_len;
                    if (score > best_score) {
                        best_score = score;
                        g_cp_off = cp_try[ci];
                        g_fn_off = nm_try[ni];
                        g_nx_off = nx_try[xi];
                        best_po_idx = poi;
                    }
                }
            }
        }
    }
    if (best_score < 1) return 0;
    g_po_off = po_cand[best_po_idx];
    printf("[+] ffield layout: ChildProps=+0x%x, Name=+0x%x, Next=+0x%x, Offset=+0x%x (score=%d)\n",
           g_cp_off, g_fn_off, g_nx_off, g_po_off, best_score);

    /* detect SuperStruct: look for a valid UStruct pointer at nearby offsets */
    {
        static const int ss_try[] = {0x40, 0x38, 0x48, 0x30, 0x50};
        for (int si = 0; si < 5; si++) {
            if (ss_try[si] == g_cp_off) continue;
            uint64_t ss = rptr(uclass + ss_try[si]);
            if (!ss || ss > 0x7FFFFFFFFFFFULL) continue;
            /* SuperStruct must be a UStruct (has vtable in module + valid name) */
            uint64_t vt = rptr(ss);
            if (vt < base || vt >= base + 0x10000000ULL) continue;
            int32_t snidx = ri32(ss + 0x18);
            char sn[128] = {0};
            if (resolve_fname(snidx, sn, sizeof(sn)) > 0) {
                /* extra verify: SuperStruct should also have ChildProperties at same offset */
                uint64_t ss_cp = rptr(ss + g_cp_off);
                if (ss_cp && ss_cp < 0x7FFFFFFFFFFFULL) {
                    g_ss_off = ss_try[si];
                    printf("[+] SuperStruct = +0x%x (parent = \"%s\")\n", g_ss_off, sn);
                    break;
                }
                /* accept even without cp if name looks right */
                g_ss_off = ss_try[si];
                printf("[+] SuperStruct = +0x%x (parent = \"%s\")\n", g_ss_off, sn);
                break;
            }
        }
    }
    return 1;
}

static void dump_class_props(uint64_t uclass, int inherited) {
    if (!uclass) return;
    char cls_name[256] = {0};
    resolve_fname(ri32(uclass + 0x18), cls_name, sizeof(cls_name));

    /* walk SuperStruct first (print parent props first) */
    if (inherited && g_ss_off >= 0) {
        uint64_t super = rptr(uclass + g_ss_off);
        if (super && super != uclass)
            dump_class_props(super, inherited);
    }


    uint64_t fp = rptr(uclass + g_cp_off);
    int count = 0;
    while (fp && fp < 0x7FFFFFFFFFFFULL && count < 500) {
        int32_t nidx = ri32(fp + g_fn_off);
        char pname[256] = {0};
        resolve_fname(nidx, pname, sizeof(pname));
        int32_t poff = ri32(fp + g_po_off);

        if (pname[0] && poff >= 0 && poff < 0x4000)
            printf("    +0x%-4x  %s\n", poff, pname);

        fp = rptr(fp + g_nx_off);
        count++;
    }
}

static void cmd_dumpclass(const char* target_class) {
    if (!init_fname_pool()) {
        printf("[-] need fnamepool\n");
        return;
    }
    uint64_t uw = get_uworld();
    if (!uw) { printf("[-] uworld null\n"); return; }

    uint64_t plevel = rptr(uw + 0x38);
    if (!plevel) { printf("[-] persistentlevel null\n"); return; }
    uint64_t actor_arr = rptr(plevel + 0xA0);
    int32_t actor_cnt  = ri32(plevel + 0xA8);
    if (!actor_arr || actor_cnt <= 0) { printf("[-] no actors\n"); return; }

    /* collect unique classes from actors */
    uint64_t classes[256] = {0};
    char     cnames[256][128];
    int nclasses = 0;

    int total = actor_cnt < 256 ? actor_cnt : 256;
    for (int i = 0; i < total; i++) {
        uint64_t actor = rptr(actor_arr + (uint64_t)i * 8);
        if (!actor) continue;
        uint64_t cls = rptr(actor + 0x10);
        if (!cls) continue;
        int dup = 0;
        for (int j = 0; j < nclasses; j++) if (classes[j] == cls) { dup = 1; break; }
        if (dup) continue;
        char cn[128] = {0};
        resolve_fname(ri32(cls + 0x18), cn, sizeof(cn));
        if (target_class && target_class[0] != '*' && !strstr(cn, target_class)) continue;
        classes[nclasses] = cls;
        memcpy(cnames[nclasses], cn, 128);
        nclasses++;
        if (nclasses >= 256) break;
    }
    if (!nclasses) return;

    /* detect layout from first class */
    if (g_cp_off < 0 && !detect_ffield_layout(classes[0])) {
        printf("[-] could not detect FField layout\n");
        return;
    }

    /* dump each class */
    for (int i = 0; i < nclasses; i++) {
        dump_class_props(classes[i], 1);
    }
}

/* sdk dump — collects all UClass pointers from actors + full SuperStruct chain */
static void cmd_sdkdump(const char* filter) {
    if (!init_fname_pool()) {
        printf("[-] need fnamepool\n");
        return;
    }

    uint64_t uw = get_uworld();
    if (!uw) { printf("[-] uworld null\n"); return; }

    uint64_t plevel = rptr(uw + 0x38);
    if (!plevel) { printf("[-] persistentlevel null\n"); return; }
    uint64_t actor_arr = rptr(plevel + 0xA0);
    int32_t actor_cnt  = ri32(plevel + 0xA8);
    if (!actor_arr || actor_cnt <= 0) { printf("[-] no actors\n"); return; }

    /* collect all UClass pointers from actors + walk full SuperStruct chains */
    uint64_t classes[2048] = {0};
    char     cnames[2048][128];
    int nclasses = 0;

    int total = actor_cnt < 512 ? actor_cnt : 512;
    for (int i = 0; i < total; i++) {
        uint64_t actor = rptr(actor_arr + (uint64_t)i * 8);
        if (!actor) continue;
        uint64_t cls = rptr(actor + 0x10);
        if (!cls || cls > 0x7FFFFFFFFFFFULL) continue;

        /* walk full SuperStruct chain to collect ALL parent classes */
        uint64_t cur = cls;
        int depth = 0;
        while (cur && cur < 0x7FFFFFFFFFFFULL && depth < 30) {
            /* dedup */
            int dup = 0;
            for (int j = 0; j < nclasses; j++) if (classes[j] == cur) { dup = 1; break; }
            if (dup) break;

            uint64_t vt = rptr(cur);
            if (vt < base || vt >= base + 0x10000000ULL) break;

            char cn[128] = {0};
            resolve_fname(ri32(cur + 0x18), cn, sizeof(cn));
            classes[nclasses] = cur;
            memcpy(cnames[nclasses], cn, 128);
            nclasses++;
            if (nclasses >= 2048) break;

            /* follow SuperStruct — use known offset if detected, else try +0x48 */
            int ss = g_ss_off >= 0 ? g_ss_off : 0x48;
            cur = rptr(cur + ss);
            depth++;
        }
        if (nclasses >= 2048) break;
    }

    if (!nclasses) return;

    /* detect layout from first class that has properties */
    if (g_cp_off < 0) {
        for (int i = 0; i < nclasses; i++) {
            if (detect_ffield_layout(classes[i])) break;
        }
        if (g_cp_off < 0) {
            printf("[-] could not detect ffield layout\n");
            return;
        }
    }

    /* filter and dump */
    int dumped = 0;
    for (int i = 0; i < nclasses; i++) {
        /* apply name filter */
        if (filter && filter[0] != '*' && !strstr(cnames[i], filter)) continue;

        /* only dump classes that have at least 1 property */
        uint64_t fp = rptr(classes[i] + g_cp_off);
        if (!fp || fp > 0x7FFFFFFFFFFFULL) continue;
        int32_t nidx = ri32(fp + g_fn_off);
        char tmp[64] = {0};
        if (resolve_fname(nidx, tmp, sizeof(tmp)) <= 0) continue;

        dump_class_props(classes[i], 0);
        dumped++;
    }
    printf("\n[*] dumped %d classes\n", dumped);
}

/* actors via UWorld — alternative to GObjects, works without a GObjects offset */

static void cmd_actors(int limit) {
    if (!init_fname_pool()) {
        printf("[-] need fnamepool for name resolution\n");
        return;
    }

    uint64_t uw = get_uworld();
    if (!uw) { printf("[-] uworld null\n"); return; }

    uint64_t vt = rptr(uw);
    if (vt < base || vt >= base + 0x10000000ULL) {
        printf("[-] uworld vtable invalid\n");
        return;
    }

    uint64_t plevel = rptr(uw + 0x38);
    if (!plevel || plevel > 0x7FFFFFFFFFFFULL) { printf("[-] PersistentLevel null\n"); return; }

    uint64_t actor_arr  = rptr(plevel + 0xA0);
    int32_t  actor_cnt  = ri32(plevel + 0xA8);
    if (!actor_arr || actor_cnt <= 0 || actor_cnt > 100000) {
        printf("[-] bad actor array\n");
        return;
    }

    int total = actor_cnt < limit ? actor_cnt : limit;
    int count = 0;
    printf("  %-6s %-18s %-24s %s\n", "Index", "Actor", "Class", "Name");
    for (int i = 0; i < total; i++) {
        uint64_t actor = rptr(actor_arr + (uint64_t)i * 8);
        if (!actor) continue;
        int32_t nidx = ri32(actor + 0x18);
        char aname[256] = {0};
        resolve_fname(nidx, aname, sizeof(aname));
        uint64_t cls = rptr(actor + 0x10);
        char cname[256] = {0};
        if (cls) {
            int32_t cnidx = ri32(cls + 0x18);
            resolve_fname(cnidx, cname, sizeof(cname));
        }
        if (aname[0] || cname[0]) {
            printf("  %-6d 0x%-16llx %-24s %s\n", i,
                   (unsigned long long)actor, cname, aname);
            count++;
        }
    }
    printf("\n[*] %d actors\n", count);
}

/* entity list — positions, hp and team id from PlayerArray */

static void cmd_entities(void) {
    uint64_t uw = get_uworld();
    if (!uw) { printf("[-] uworld null\n"); return; }

    uint64_t game_state = rptr(uw + GameState);
    if (!game_state) { printf("[-] gamestate null\n"); return; }

    uint64_t arr_data  = rptr(game_state + PlayerArray);
    int      arr_count = ri32(game_state + PlayerArray + 8);
    if (arr_count <= 0 || arr_count > 100) { printf("[-] bad count\n"); return; }

    printf("  %-4s %-12s %-12s %-12s %-6s %-6s\n", "idx", "X", "Y", "Z", "HP", "Team");
    for (int i = 0; i < arr_count; i++) {
        uint64_t ps = rptr(arr_data + i * 8);
        if (!ps) continue;
        uint64_t pawn = rptr(ps + PawnPrivate);
        if (!pawn) continue;
        uint64_t root = rptr(pawn + RootComponent);
        vec3 pos = {0,0,0};
        if (root) pos = rvec3(root + RelativeLoc);
        uint64_t dmg = rptr(pawn + DamageHandler);
        int hp = dmg ? ri32(dmg + Health) : -1;
        uint64_t tcomp = rptr(pawn + TeamComponent);
        int team = tcomp ? ri32(tcomp + TeamId) : -1;
        printf("  %-4d %-12.1f %-12.1f %-12.1f %-6d %-6d\n", i, pos.x, pos.y, pos.z, hp, team);
    }
}

/* entry point — parse command line and dispatch to the relevant command */

int main(int argc, char* argv[]) {
    int do_gnames = 0, do_entities = 0, do_gobjects = 0, do_actors = 0, do_pakoffs = 0, do_patch = 0;
    const char* dump_class = NULL;
    const char* sdk_filter = NULL;
    int gnames_limit = 2000;
    uint64_t hexdump_addr = 0;
    int      hexdump_sz   = 64;
    uint64_t patch_addr   = 0;
    uint8_t  patch_bytes[32] = {0};
    int      patch_len = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "gnames"))     do_gnames = 1;
        if (!strcmp(argv[i], "entities"))   do_entities = 1;
        if (!strcmp(argv[i], "all"))      { do_gnames = do_entities = 1; }
        if (!strcmp(argv[i], "gobjects"))   do_gobjects = 1;
        if (!strcmp(argv[i], "actors"))     do_actors = 1;
        if (!strcmp(argv[i], "dumpclass"))  { dump_class = (i+1 < argc) ? argv[++i] : "*"; }
        if (!strcmp(argv[i], "sdkdump"))    { sdk_filter = (i+1 < argc) ? argv[++i] : "*"; }
        if (!strcmp(argv[i], "pakoffsets")) do_pakoffs = 1;
        if (!strcmp(argv[i], "sections"))   do_pakoffs = 2;
        if (!strcmp(argv[i], "hd") && i + 1 < argc) {
            hexdump_addr = (uint64_t)strtoull(argv[++i], NULL, 16);
            if (i + 1 < argc && argv[i+1][0] >= '0' && argv[i+1][0] <= '9')
                hexdump_sz = atoi(argv[++i]);
        }
        if (!strcmp(argv[i], "patch") && i + 2 < argc) {
            do_patch = 1;
            patch_addr = (uint64_t)strtoull(argv[++i], NULL, 16);
            while (i + 1 < argc && patch_len < 32) {
                char* end; unsigned long v = strtoul(argv[i+1], &end, 16);
                if (end == argv[i+1]) break;
                patch_bytes[patch_len++] = (uint8_t)v; i++;
            }
        }
        if (!strcmp(argv[i], "-n") && i + 1 < argc) gnames_limit = atoi(argv[++i]);
    }

    if (!do_gnames && !do_entities && !do_gobjects && !do_actors && !dump_class && !sdk_filter && !do_pakoffs && !hexdump_addr && !do_patch) {
        printf("usage: val_reader [command] [options]\n\n");
        printf("  gnames          dump FNamePool entries\n");
        printf("  entities        dump player positions, HP, team\n");
        printf("  gobjects        dump GObjects with class names\n");
        printf("  actors          dump actors via UWorld\n");
        printf("  dumpclass [name] dump class properties (* = all)\n");
        printf("  sdkdump [filter] dump ALL classes via GObjects\n");
        printf("  all             gnames + entities\n");
        printf("  pakoffsets      scan for PAK bypass offsets\n");
        printf("  sections        dump PE section table\n");
        printf("  hd <addr> [N]   hex dump N bytes (default 64)\n");
        printf("  patch <a> <b..> write bytes to address\n");
        printf("  -n N            limit gnames (default 2000)\n");
        return 1;
    }

    DWORD pid = find_pid(L"VALORANT-Win64-Shipping.exe");
    if (!pid) { printf("[-] Valorant not found\n"); return 1; }
    printf("[*] PID = %lu\n", pid);

    base = find_module_base(pid, L"VALORANT-Win64-Shipping.exe");
    if (!base) { printf("[-] module base failed\n"); return 1; }
    printf("[*] base = 0x%llx\n\n", (unsigned long long)base);

    enable_debug_priv();

    hProc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) { printf("[-] OpenProcess failed (%lu)\n", GetLastError()); return 1; }

    /* patch */
    if (do_patch) {
        uint64_t va = (patch_addr >= base) ? patch_addr : base + patch_addr;
        printf("--- patch @ 0x%llx (%d bytes) ---\n", (unsigned long long)va, patch_len);
        HANDLE pw = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_VM_READ, FALSE, pid);
        if (!pw) { printf("[-] open failed\n"); goto skip_patch; }

        uint8_t before[32] = {0};
        SIZE_T brd = 0;
        ReadProcessMemory(pw, (LPCVOID)va, before, patch_len, &brd);
        printf("[*] before: "); for (int j = 0; j < patch_len; j++) printf("%02X ", before[j]); printf("\n");

        MEMORY_BASIC_INFORMATION mbi = {0};
        VirtualQueryEx(pw, (LPCVOID)va, &mbi, sizeof(mbi));
        DWORD old_prot = 0;
        DWORD new_prot = (mbi.Type == MEM_MAPPED) ? PAGE_EXECUTE_WRITECOPY : PAGE_EXECUTE_READWRITE;
        VirtualProtectEx(pw, (LPVOID)va, patch_len, new_prot, &old_prot);

        SIZE_T written = 0;
        if (!WriteProcessMemory(pw, (LPVOID)va, patch_bytes, patch_len, &written))
            printf("[-] write failed (%lu)\n", GetLastError());
        else {
            printf("[+] written %zu bytes\n", (size_t)written);
            uint8_t after[32] = {0};
            ReadProcessMemory(pw, (LPCVOID)va, after, patch_len, &brd);
            printf("[*] after:  "); for (int j = 0; j < patch_len; j++) printf("%02X ", after[j]); printf("\n");
        }
        if (old_prot) VirtualProtectEx(pw, (LPVOID)va, patch_len, old_prot, &old_prot);
        CloseHandle(pw);
        skip_patch: printf("\n");
    }

    /* hexdump */
    if (hexdump_addr) {
        uint64_t va = (hexdump_addr >= 0x100000ULL) ? hexdump_addr : base + hexdump_addr;
        printf("--- hexdump @ 0x%llx (%d bytes) ---\n", (unsigned long long)va, hexdump_sz);
        uint8_t* hbuf = (uint8_t*)malloc(hexdump_sz);
        SIZE_T hrd = 0;
        if (!hbuf || !ReadProcessMemory(hProc, (LPCVOID)va, hbuf, hexdump_sz, &hrd)) {
            printf("[-] read failed (%lu)\n", GetLastError());
        } else {
            for (int row = 0; row < (int)hrd; row += 16) {
                printf("  %04x  ", row);
                for (int col = 0; col < 16; col++) {
                    if (row+col < (int)hrd) printf("%02X ", hbuf[row+col]); else printf("   ");
                    if (col == 7) printf(" ");
                }
                printf(" |");
                for (int col = 0; col < 16 && row+col < (int)hrd; col++) {
                    uint8_t c = hbuf[row+col];
                    printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
                }
                printf("|\n");
            }
        }
        free(hbuf); printf("\n");
    }

    /* sections */
    if (do_pakoffs == 2) {
        uint8_t dos[0x40];
        SIZE_T rd2 = 0;
        ReadProcessMemory(hProc, (LPCVOID)base, dos, sizeof(dos), &rd2);
        int32_t pe_off2 = *(int32_t*)(dos + 0x3C);
        uint8_t pe2[0x400] = {0};
        ReadProcessMemory(hProc, (LPCVOID)(base + pe_off2), pe2, sizeof(pe2), &rd2);
        uint16_t nsec2 = *(uint16_t*)(pe2 + 6);
        uint16_t oph2  = *(uint16_t*)(pe2 + 20);
        uint8_t* secs2 = pe2 + 24 + oph2;
        printf("--- PE sections ---\n");
        printf("  %-8s  %10s  %10s  %10s  flags\n", "Name", "VirtAddr", "VirtSize", "VA");
        for (int s = 0; s < nsec2 && s < 64; s++) {
            uint8_t* sec = secs2 + s * 40;
            char nm[9] = {0};
            memcpy(nm, sec, 8);
            uint32_t vr2 = *(uint32_t*)(sec + 12);
            uint32_t vs2 = *(uint32_t*)(sec + 16);
            uint32_t ch2 = *(uint32_t*)(sec + 36);
            printf("  %-8s  %10x  %10x  %10llx  %c%c%c\n", nm, vr2, vs2,
                   (unsigned long long)(base + vr2),
                   (ch2 & 0x20000000)?'X':'-', (ch2 & 0x40000000)?'R':'-', (ch2 & 0x80000000)?'W':'-');
        }
        printf("\n");
    }

    /* pakoffsets */
    if (do_pakoffs == 1) {
        printf("--- PAK offset scan ---\n");
        struct { const char* name; const uint8_t* pat; const char* msk; int len; int rip_off; } pats[] = {
            { "bypass_pak_signing",
              (const uint8_t[]){0x84,0xC0,0x74,0,0xB0,0x01}, "xxx?xx", 6, -1 },
            { "fpak_platform_file",
              (const uint8_t[]){0x48,0x8B,0x0D,0,0,0,0,0x48,0x85,0xC9,0x0F,0x84,0,0,0,0,
                                0x48,0x8B,0x01,0x48,0x8B,0x40,0,0xFF,0x15,0,0,0,0},
              "xxx????xxxxx????xxxxxx?xx????", 29, 3 },
            { "mount_fn",
              (const uint8_t[]){0x48,0x81,0xEC,0,0,0,0,0x0F,0x29}, "xxx????xx", 9, -1 },
        };
        uint8_t* sbuf = (uint8_t*)malloc(65536);
        if (sbuf) {
            for (int pi = 0; pi < 3; pi++) {
                int found = 0;
                uint64_t scan_va = base + 0x1000;
                size_t scan_sz = 150 * 1024 * 1024;
                for (size_t so = 0; so < scan_sz && !found; so += 65536 - pats[pi].len) {
                    SIZE_T sg = 0;
                    if (!ReadProcessMemory(hProc, (LPCVOID)(scan_va+so), sbuf, 65536, &sg)) continue;
                    for (size_t si = 0; si + pats[pi].len <= sg && !found; si++) {
                        int ok = 1;
                        for (int j = 0; j < pats[pi].len; j++)
                            if (pats[pi].msk[j]=='x' && sbuf[si+j] != pats[pi].pat[j]) { ok=0; break; }
                        if (!ok) continue;
                        uint64_t va = scan_va + so + si;
                        uint64_t result = va;
                        if (pats[pi].rip_off >= 0) {
                            int32_t disp = *(int32_t*)(sbuf + si + pats[pi].rip_off);
                            result = va + pats[pi].rip_off + 4 + disp;
                        }
                        printf("  [+] %-24s offset=0x%llx\n", pats[pi].name,
                               (unsigned long long)(result - base));
                        found = 1;
                    }
                }
                if (!found) printf("  [-] %-24s not found\n", pats[pi].name);
            }
            free(sbuf);
        }
        printf("\n");
    }

    if (do_gnames)   { printf("--- GNames ---\n"); cmd_gnames(gnames_limit); printf("\n"); }
    if (do_gobjects) { printf("--- GObjects ---\n"); cmd_gobjects(gnames_limit); printf("\n"); }
    if (do_actors)   { printf("--- Actors ---\n"); cmd_actors(gnames_limit); printf("\n"); }
    if (dump_class)  { printf("--- Class Dump ---\n"); cmd_dumpclass(dump_class); printf("\n"); }
    if (sdk_filter)  { printf("--- SDK Dump ---\n"); cmd_sdkdump(sdk_filter); printf("\n"); }
    if (do_entities) { printf("--- Entities ---\n"); cmd_entities(); printf("\n"); }

    CloseHandle(hProc);
    return 0;
}

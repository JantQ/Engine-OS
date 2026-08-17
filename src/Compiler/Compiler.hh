#pragma once

#include "../kernel.hh"
#include "../Script/Script.hh"
#include "../Libs/Crc.hh"

#define ENEXE_MAGIC_BYTES 8
#define ENEXE_FORMAT 1
#define ENEXE_CTX_VERSION 1
#define ENEXE_MAX_BYTES (64 * 1024)

static const UINT8 ENEXE_MAGIC[ENEXE_MAGIC_BYTES] = {
    'E','n','E','x','E','1','!','\0'
};

struct EnCtx {
    INT64 vars[SCRIPT_MAX_VARS];
    INT64 *arena;
    UINT64 arenaUsed;
    UINT64 arenaSlots;
    UINT64 rngState;
    INT64 fuel;
    const char *strings;
    void *fn[16];
};

#define CTX_VARS ((UINT32)__builtin_offsetof(EnCtx, vars))
#define CTX_RNG ((UINT32)__builtin_offsetof(EnCtx, rngState))

struct __attribute__((packed)) EnExeHeader {
    UINT8 magic[ENEXE_MAGIC_BYTES];
    UINT32 formatVersion;
    UINT32 ctxVersion;
    UINT64 codeOffset;
    UINT64 codeBytes;
    UINT64 entryOffset;
    UINT64 stringOffset;
    UINT64 stringBytes;
    UINT32 varCount;
    UINT32 crc32;
};

static inline UINT32 EnExeCrc(const UINT8 *image, UINT64 total) {
    return Crc::Crc32(image + sizeof(EnExeHeader), total - sizeof(EnExeHeader));
}

typedef void (*EnCode)(EnCtx *ctx);

class Compiler {
    public:
        static UINT64 Compile(UINT8 *out, UINT64 cap, UINT32 *badLine);
};
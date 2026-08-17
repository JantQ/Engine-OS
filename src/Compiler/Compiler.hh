#pragma once

#include "../kernel.hh"
#include "../Script/Script.hh"
#include "../Libs/Crc.hh"

#define ENEXE_MAGIC_BYTES 8
#define ENEXE_FORMAT 1
#define ENEXE_CTX_VERSION 2
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
    INT64 args[8];
};

struct Fixup {
    UINT64 site;
    INT32 target;
};

#define CTX_VARS ((UINT32)__builtin_offsetof(EnCtx, vars))
#define CTX_RNG ((UINT32)__builtin_offsetof(EnCtx, rngState))
#define CTX_FN ((UINT32)__builtin_offsetof(EnCtx, fn))
#define CTX_ARGS ((UINT32)__builtin_offsetof(EnCtx, args))

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

enum EnFn : UINT32 {
    FN_KEYDOWN = 0,
    FN_SCREENW = 1,
    FN_SCREENH = 2,
    FN_MILLIS = 3,
    FN_WAIT = 4,
    FN_RECT = 5,
    FN_TEXT = 6,
    FN_NUM = 7,
    FN_FLIP = 8,
    FN_ALLOC = 9,
    FN_POKE = 10,
    FN_PEEK = 11,
    FN_COUNT
};

static inline UINT32 EnExeCrc(const UINT8 *image, UINT64 total) {
    return Crc::Crc32(image + sizeof(EnExeHeader), total - sizeof(EnExeHeader));
}

static UINT32 lineOffset[SCRIPT_MAX_LINES + 1];
static Fixup fixups[SCRIPT_MAX_LINES];
static UINT32 fixupCount = 0;

typedef void (*EnCode)(EnCtx *ctx);

class Compiler {
    public:
        static UINT64 Compile(UINT8 *out, UINT64 cap, UINT32 *badLine);
};
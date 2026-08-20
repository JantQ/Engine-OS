#include "Compiler.hh"
#include "Loader.hh"
#include "../Memory/Framse.hh"
#include "../Clock//Clock.hh"
#include "../Engine/Engine.hh"
#include "../input/keyboard.hh"
#include "../Graphics/graphics.hh"

LoadResult Loader::RunImage(const UINT8 *image, UINT64 size) {
    if (size < sizeof(EnExeHeader)) return LOAD_TOO_SMALL;

    const EnExeHeader *header = (const EnExeHeader*)image;

    for (UINT32 i = 0; i < ENEXE_MAGIC_BYTES; i++) {
        if (header->magic[i] != ENEXE_MAGIC[i]) return LOAD_BAD_MAGIC;
    }

    if (header->formatVersion != ENEXE_FORMAT) return LOAD_BAD_FORMAT;
    if (header->ctxVersion != ENEXE_CTX_VERSION) return LOAD_BAD_CTX;

    if (header->codeBytes > size || header->codeOffset > size - header->codeBytes || header->stringBytes > size
        || header->stringOffset > size - header->stringBytes || header->entryOffset >= header->codeBytes) {
            return LOAD_BAD_HEADER;
    }

    if (EnExeCrc(image, size) != header->crc32) return LOAD_BAD_CRC;

    UINT64 frames = (header->codeBytes + FRAME_SIZE - 1) / FRAME_SIZE;
    UINT8 *code = (UINT8*)Frames::Alloc(frames);
    if (!code) return LOAD_NO_PAGES;

    for (UINT64 i = 0; i < header->codeBytes; i++) {
        code[i] = image[header->codeOffset + i];
    }

    for (UINT32 i = 0; i < SCRIPT_MAX_VARS; i++) ctx.vars[i] = 0;

    ctx.arena = 0;
        ctx.arenaUsed = 0;
    ctx.arenaSlots = 0;
    ctx.rngState = Clock::rdtsc() | 1;
    ctx.fuel = 200000;
    ctx.strings = (const char*)(image + header->stringOffset);
    ctx.fn[FN_KEYDOWN] = (void*)&EnKeyDown;
    ctx.fn[FN_SCREENW] = (void*)&EnScreenW;
    ctx.fn[FN_SCREENH] = (void*)&EnScreenH;
    ctx.fn[FN_MILLIS] = (void*)&EnMillis;
    ctx.fn[FN_WAIT] = (void*)&EnWait;
    ctx.fn[FN_RECT] = (void*)&EnRect;
    ctx.fn[FN_TEXT] = (void*)&EnText;
    ctx.fn[FN_NUM] = (void*)&EnNum;
    ctx.fn[FN_FLIP] = (void*)&EnFlip;
    ctx.fn[FN_ALLOC] = (void*)&EnAlloc;
    ctx.fn[FN_POKE] = (void*)&EnPoke;
    ctx.fn[FN_PEEK] = (void*)&EnPeek;

    Keyboard::ClearStates();
    Graphics::back.clear(0x00000000);
    EnCode entry = (EnCode)(void*)(code + header->entryOffset);
    entry(&ctx);

    if (ctx.arena) {
        Frames::Free(ctx.arena, SCRIPT_ARENA_BYTES / FRAME_SIZE);
        ctx.arena = 0;
        ctx.arenaUsed = 0;
        ctx.arenaSlots = 0;
    }

    Frames::Free(code, frames);
    return LOAD_OK;
}
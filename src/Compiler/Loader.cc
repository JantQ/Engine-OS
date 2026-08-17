#include "Loader.hh"
#include "../Console/Console.hh"
#include "../FileSystem/EnFS.hh"
#include "../Memory/Heap.hh"
#include "../Memory/Framse.hh"
#include "../Clock/Clock.hh"
#include "Compiler.hh"
#include "../Engine/Engine.hh"
#include "../input/keyboard.hh"
#include "../Graphics/graphics.hh"

static UINT8 *loadBuffer = 0;
static EnCtx runCtx;

bool Loader::Run(const char *name) {
    if (!EnFS::mounted) return false;
    if (!EnFS::Find(name)) return false;

    if (!loadBuffer) loadBuffer = (UINT8*)Heap::Alloc(ENEXE_MAX_BYTES, 16);
    if (!loadBuffer) {
        Console::Println("alloc failed");
        return true;
    }

    UINT64 size = 0;
    if (EnFS::ReadFile(name, loadBuffer, ENEXE_MAX_BYTES, &size) != ENFS_OK) return false;
    if (size < sizeof(EnExeHeader)) return false;

    EnExeHeader *header = (EnExeHeader*)loadBuffer;

    for (UINT32 i = 0; i < ENEXE_MAGIC_BYTES; i++) {
        if (header->magic[i] != ENEXE_MAGIC[i]) return false;
    }

    if (header->formatVersion != ENEXE_FORMAT) {
        Console::Println("Game: Wrong format version");
        return true;
    }
    if (header->ctxVersion != ENEXE_CTX_VERSION) {
        Console::Println("Game: Built for a different kernel version");
        return true;
    }

    if (header->codeBytes > size || header->codeOffset > size - header->codeBytes || header->stringBytes > size
        || header->stringOffset > size - header->stringBytes
        || header->entryOffset >= header->codeBytes) {
            Console::Println("Game: Bad header");
            return true;
    }

    if (EnExeCrc(loadBuffer, size) != header->crc32) {
        Console::Println("Game: Bad crc");
        return true;
    }

    UINT64 frames = (header->codeBytes + FRAME_SIZE - 1) / FRAME_SIZE;
    UINT8 *code = (UINT8*)Frames::Alloc(frames);

    if (!code) {
        Console::Println("Game: No code pages");
        return true;
    }

    for (UINT64 i = 0; i < header->codeBytes; i++) {
        code[i] = loadBuffer[header->codeOffset + i];
    }

    for (UINT32 i = 0; i < SCRIPT_MAX_VARS; i++) runCtx.vars[i] = 0;

    runCtx.arena = 0;
    runCtx.arenaUsed = 0;
    runCtx.arenaSlots = 0;
    runCtx.rngState = Clock::rdtsc() | 1;
    runCtx.fuel = 200000;
    runCtx.strings = (const char*)(loadBuffer + header->stringOffset);
    runCtx.fn[FN_KEYDOWN] = (void*)&EnKeyDown;
    runCtx.fn[FN_SCREENW] = (void*)&EnScreenW;
    runCtx.fn[FN_SCREENH] = (void*)&EnScreenH;
    runCtx.fn[FN_MILLIS] = (void*)&EnMillis;
    runCtx.fn[FN_WAIT] = (void*)&EnWait;
    runCtx.fn[FN_RECT] = (void*)&EnRect;
    runCtx.fn[FN_TEXT] = (void*)&EnText;
    runCtx.fn[FN_NUM] = (void*)&EnNum;
    runCtx.fn[FN_FLIP] = (void*)&EnFlip;
    runCtx.fn[FN_ALLOC] = (void*)&EnAlloc;
    runCtx.fn[FN_POKE] = (void*)&EnPoke;
    runCtx.fn[FN_PEEK] = (void*)&EnPeek;
    

    Keyboard::ClearStates();
    Graphics::back.clear(0x00000000);

    EnCode entry = (EnCode)(void*)(code + header->entryOffset);
    entry(&runCtx);

    if (runCtx.arena) {
        Frames::Free(runCtx.arena, SCRIPT_ARENA_BYTES / FRAME_SIZE);
        runCtx.arena = 0;
        runCtx.arenaUsed = 0;
        runCtx.arenaSlots = 0;
    }

    for (UINT32 i = 0; i < header->varCount && i < SCRIPT_MAX_VARS; i++) {
        Console::PrintUInt(i);
        Console::Print(" = ");
        Console::PrintUInt((UINT64)runCtx.vars[i]);
        Console::Print('\n');
    }

    Frames::Free(code, frames);
    return true;
}
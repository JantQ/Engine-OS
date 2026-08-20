#include "Loader.hh"
#include "../Console/Console.hh"
#include "../FileSystem/EnFS.hh"
#include "../Memory/Heap.hh"
#include "Compiler.hh"

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
    
    LoadResult r = RunImage(loadBuffer, size);

    if (r == LOAD_TOO_SMALL || r == LOAD_BAD_MAGIC) return false;

    if (r != LOAD_OK) {
        Console::Print("Game: ");
        Console::Println(ResultName(r));
        return true;
    }

    const EnExeHeader *header = (const EnExeHeader*)loadBuffer;
    for (UINT32 i = 0; i < header->varCount && i < SCRIPT_MAX_VARS; i++) {
        Console::PrintUInt(i);
        Console::Print(" = ");
        Console::PrintUInt((UINT64)ctx.vars[i]);
        Console::Print('\n');
    }

    return true;
}
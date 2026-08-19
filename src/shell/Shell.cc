#include "Shell.hh"
#include "../Libs/Str.hh"
#include "../Console/Console.hh"
#include "../PCIe/storage/Nvme.hh"
#include "../Memory/Heap.hh"
#include "../PCIe/storage/Gpt.hh"
#include "../PCIe/storage/part.hh"
#include "../Editor/Editor.hh"
#include "../FileSystem/EnFS.hh"
#include "../Script/Script.hh"
#include "../Memory/Framse.hh"
#include "../Memory/Paging.hh"
#include "../Compiler/Compiler.hh"
#include "../Compiler/Loader.hh"
#include "../Libs/Crc.hh"
#include "../Lang/parse.hh"


extern "C" UINT8 _binary_RUNTIME_EFI_start[];
extern "C" UINT8 _binary_RUNTIME_EFI_end[];

static UINT8 *sectorBuffer = 0;
static UINT8 *partBuffer = 0;
static UINT8 *scriptBuffer = 0;
static UINT8 *exportBuffer = 0;
static UINT8 *buildBuffer = 0;

void Shell::Execute(const char *line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

    char name[32];

    UINT32 i = 0;
    while (line[i] && line[i] != ' ' && i < 31) {
        name[i] = line[i];
        i++;
    }
    name[i] = '\0';

    const char *args = line + i;
    while (*args == ' ') args++;

    for (UINT32 c = 0; c < commandCount; c++) {
        if (Str::Equal(name, commands[c].name)) {
            Console::Print('\n');
            commands[c].handler(args);
            return;
        }
    }
    Console::Print('\n');

    if (Loader::Run(name)) return;

    Console::Print("Unknown command: ");
    Console::Println(name);
}

static void CmdHelp(const char *) {
    for (UINT32 i = 0; i < Shell::commandCount; i++) {
        Console::Print(Shell::commands[i].name);
        Console::Print(" - ");
        Console::Println(Shell::commands[i].help);

    }
}

static void CmdClear(const char *) {
    Console::Clear();
}

static void CmdEcho(const char *args) {
    Console::Println(args);
}

static void CmdRead(const char *args) {
    if (Nvme::nsid == 0) {
        Console::Println("no namespace");
        return;
    }

    if (!sectorBuffer) {
        sectorBuffer = (UINT8*)Heap::Alloc(4096, 4096);
    }
    if (!sectorBuffer) {
        Console::Println("alloc failed");
        return;
    }

    UINT64 lba = Str::ToUInt(args);

    if (!Nvme::ReadBlocks(Nvme::nsid, lba, 1, sectorBuffer)) {
        Console::Println("read failed");
        return;
    }

    for (UINT32 row = 0; row < 8; row++) {
        Console::PrintHex(row * 16, 4);
        Console::Print(" ");
        for (UINT32 b = 0; b < 16; b++) {
            Console::PrintHex(sectorBuffer[row * 16 + b],2);
            Console::Print(' ');
        }
        Console::Print('\n');
    }

}

static void CmdGpt(const char *) {
    GptResult result = Gpt::Parse();

    if (result != GPT_OK) {
        Console::Print("gpt: ");
        Console::Println(Gpt::ResultName(result));
        return;
    }

    Console::Print("usable ");
    Console::PrintUInt(Gpt::firstUsable);
    Console::Print(" - ");
    Console::PrintUInt(Gpt::lastUsable);
    Console::Print('\n');

    for (UINT32 i = 0; i < Gpt::count; i++) {
        GptEntry &entry = Gpt::parts[i];
        UINT64 mib = (entry.endingLba - entry.startingLba + 1) * Nvme::blockSize / MiB;

        Console::PrintUInt(i);
        Console::Print(" ");
        Console::PrintUInt(entry.startingLba);
        Console::Print("-");
        Console::PrintUInt(entry.endingLba);
        Console::Print(" ");
        Console::PrintUInt(mib);
        Console::Print("M ");
        
        for (UINT32 c = 0; c < 36 && entry.name[c]; c++) {
            Console::Print((char)entry.name[c]);
        }
        Console::Print('\n');
    }
}

static UINT32 PrintGap(UINT64 start, UINT64 end, UINT64 align) {
    UINT64 alignedStart = (start + align - 1) / align * align;
    if (alignedStart > end) return 0;

    UINT64 sectors = end - alignedStart + 1;
    sectors = sectors / align * align;
    if (sectors == 0) return 0;

    Console::PrintUInt(alignedStart);
    Console::Print("-");
    Console::PrintUInt(alignedStart + sectors - 1);
    Console::Print(" ");
    Console::PrintUInt(sectors * Nvme::blockSize / MiB);
    Console::Println("M free");
    return 1;
}

static void SortedOrder(UINT32 *order) {
    for (UINT32 i = 0; i < Gpt::count; i++) order[i] = i;

    for (UINT32 i = 1; i < Gpt::count; i++) {
        UINT32 key = order[i];
        UINT32 j = i;
        while(j > 0 && Gpt::parts[order[j - 1]].startingLba > Gpt::parts[key].startingLba) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = key;
    } 
}

static bool FindGap(UINT64 need, UINT64 align, UINT64 *outStart, UINT64 *outSectors) {
    UINT32 order[Gpt::MAX_PARTS];
    SortedOrder(order);

    UINT64 cursor = Gpt::firstUsable;

    for (UINT32 i = 0; i <= Gpt::count; i++) {
        UINT64 limit = (i < Gpt::count) ? Gpt::parts[order[i]].startingLba : Gpt::lastUsable + 1;

        if (limit > cursor) {
            UINT64 start = (cursor + align - 1) / align * align;

            if (start < limit) {
                UINT64 sectors = (limit - start) / align * align;
                if (sectors > 0 && sectors >= need) {
                    *outStart = start;
                    *outSectors = sectors;
                    return true;
                }
            }
        }

        if (i < Gpt::count) {
            UINT64 next = Gpt::parts[order[i]].endingLba + 1;
            if (next > cursor) cursor = next;
        }
    }

    return false;
}

static bool IsEnFile(const char *name) {
    UINT32 n = 0;
    while (name[n]) n++;
    return n >= 3 && name[n-3] == '.' && name[n-2] == 'e' && name[n-1] == 'n';
}

static void CmdFree(const char *) {
    GptResult result = Gpt::Parse();

    if (result != GPT_OK) {
        Console::Print("gpt: ");
        Console::Println(Gpt::ResultName(result));
        return;
    }

    if (Gpt::truncated) {
        Console::Println("table truncated");
        return;
    }

    UINT32 order[Gpt::MAX_PARTS];
    SortedOrder(order);

    UINT64 align = MiB / Nvme::blockSize;
    if (align == 0) align = 1;

    UINT64 cursor = Gpt::firstUsable;
    UINT32 found = 0;

    for (UINT32 i = 0; i <= Gpt::count; i++) {
        UINT64 limit = (i < Gpt::count) ? Gpt::parts[order[i]].startingLba : Gpt::lastUsable + 1;

        if (limit > cursor) found += PrintGap(cursor, limit - 1, align);

        if (i < Gpt::count) {
            UINT64 next = Gpt::parts[order[i]].endingLba + 1;
            if (next > cursor) cursor = next;
        }
    }

    if (found == 0) Console::Println("no free space");
}

static void CmdMkpart(const char *args) {
    UINT64 sizeMiB = Str::ToUInt(args);
    if (sizeMiB == 0) {
        Console::Println("usage: mkpart <sizeMiB> <name> [--yes]");
        return;
    }

    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;

    char name[36];
    UINT32 n = 0;
    while (*args && *args != ' ' && n < 35) name[n++] = *args++;
    name[n] = '\0';

    if (n == 0) {
        Console::Println("usage: mkpart <sizeMiB> <name> [--yes]");
        return;
    }

    while (*args == ' ') args++;
    bool confirmed = Str::Equal(args, "--yes");

    GptResult result = Gpt::Parse();
    if (result != GPT_OK) {
        Console::Print("gpt: ");
        Console::Println(Gpt::ResultName(result));
        return;
    }

    UINT64 align = MiB / Nvme::blockSize;
    if (align == 0) align = 1;

    UINT64 need = sizeMiB * align;
    UINT64 start = 0;
    UINT64 avail = 0;

    if (!FindGap(need, align, &start, &avail)) {
        Console::Println("no gap large enough");
        return;
    }

    UINT64 endLba = start + need - 1;

    Console::Print("drive: ");
    Console::Println(Nvme::model);
    Console::Print("create: ");
    Console::PrintUInt(sizeMiB);
    Console::Print("M lba ");
    Console::PrintUInt(start);
    Console::Print("  name ");
    Console::Println(name);

    if (!confirmed) {
        Console::Println("add --yes to commit");
        return;
    }

    result = Gpt::CreatePartition(start, endLba, ENGINE_TYPE_GUID, name);
    Console::Print("mkpart: ");
    Console::Println((Gpt::ResultName(result)));
}

static void CmdMount(const char *) {
    if (!Part::Mount(ENGINE_TYPE_GUID)) {
        Console::Println("no engine partition");
        return;
    }

    Console::Print("mounted lba ");
    Console::PrintUInt(Part::baseLba);
    Console::Print(" size ");
    Console::PrintUInt(Part::SizeBytes() / MiB);
    Console::Println("M");
}

static void CmdPartRead(const char *args) {
    if (!partBuffer) partBuffer = (UINT8*)Heap::Alloc(4096, 4096);
    if (!partBuffer) {
        Console::Println("alloc failed");
        return;
    }

    UINT64 offset = Str::ToUInt(args);

    if (!Part::Read(offset, 1, partBuffer)) {
        Console::Println("read failed");
        return;
    }

    for (UINT32 row = 0; row < 8; row++) {
        Console::PrintHex(row * 16, 4);
        Console::Print(" ");
        for (UINT32 b = 0; b < 16; b++) {
            Console::PrintHex(partBuffer[row * 16 + b], 2);
            Console::Print(' ');
        }
        Console::Print('\n');
    }
}

static void CmdPartWrite(const char *args) {
    if (!partBuffer) partBuffer = (UINT8*)Heap::Alloc(4096, 4096);
    if (!partBuffer) {
        Console::Println("alloc failed");
        return;
    }

    UINT64 offset = Str::ToUInt(args);
    while (*args >= '0' && *args <= '9') args++;
    while (*args == ' ') args++;

    for (UINT32 i = 0; i < Nvme::blockSize; i++) {
        partBuffer[i] = 0;
    }
    
    for (UINT32 i = 0; args[i] && i < Nvme::blockSize - 1; i++) {
        partBuffer[i] = (UINT8)args[i];
    }

    Console::Println(Part::Write(offset, 1, partBuffer) ? "ok" : "write failed");
}

static void CmdWipe(const char *args) {
    if (!Part::mounted) {
        Console::Println("not mounted");
        return;
    }

    Console::Print("will zero ");
    Console::PrintUInt(Part::sectors);
    Console::Print(" sectors (");
    Console::PrintUInt(Part::SizeBytes() / MiB);
    Console::Print("M) at lba ");
    Console::PrintUInt(Part::baseLba);
    Console::Print('\n');

    if (!Str::Equal(args, "--yes")) {
        Console::Println("Add --yes to erase");
        return;
    }

    bool ok = Part::Zero(0, Part::sectors);
    Console::Println(ok ? "partition wiped" : "wipe failed");
}

static void CmdEditor(const char *args) {
    if (!EnFS::mounted) {
        Console::Println("enfs not mounted"); 
        return;
    }
    if (!*args) {
        Console::Println("usage: neenor <name>"); 
        return;
    }
    Editor::Open(args);
}

static void CmdFormat(const char *args) {
    if (!Part::mounted) {
        Console::Println("no partition mounted");
        return;
    }

    if (!Str::Equal(args, "--yes")) {
        Console::Print("will erase all files on ");
        Console::PrintUInt(Part::SizeBytes() / MiB);
        Console::Println("M - add --yes to commit");
        return;
    }

    EnFsResult r = EnFS::Format();
    Console::Print("format: ");
    Console::Println(EnFS::ResultName(r));
}

static void CmdLs(const char *) {
    if (!EnFS::mounted) {
        Console::Println("enfs not mounted");
        return;
    }

    UINT32 shown = 0;
    for (UINT32 i = 0; i < ENFS_MAX_FILES; i++) {
        if (EnFS::files[i].flags != ENFS_USED) continue;

        Console::Print(EnFS::files[i].name);
        Console::Print("  ");
        Console::PrintUInt(EnFS::files[i].byteLenght);
        Console::Println(" bytes");
        shown++;
    }

    if (shown == 0) Console::Println("no files");

    Console::Print("used ");
    Console::PrintUInt(EnFS::super.dataUsed * Part::BlockSize() / MiB);
    Console::Print("M of ");
    Console::PrintUInt(Part::SizeBytes() / MiB);
    Console::Println("M");
}

static void CmdMem(const char *) {
    UINT64 usedFrames = Frames::totalFrames - Frames::freeFrames;

    Console::Print("frames: ");
    Console::PrintUInt(usedFrames);
    Console::Print(" used of ");
    Console::PrintUInt(Frames::totalFrames);
    Console::Print(" (");
    Console::PrintUInt(usedFrames * FRAME_SIZE / MiB);
    Console::Print("M of ");
    Console::PrintUInt(Frames::totalFrames * FRAME_SIZE / MiB);
    Console::Println("M)");

    Console::Print("heap: ");
    Console::PrintUInt(Heap::Used() / 1024);
    Console::Print("K used of ");
    Console::PrintUInt(Heap::Total() / MiB);
    Console::Println("M");

    Console::Print("paging: ");
    Console::Println(Paging::ownTables ? "own tables" : "uefi tables");
}

#define RM_MAX_NAMES 16

static void CmdRm(const char *args) {
    if (!EnFS::mounted) {
        Console::Println("enfs not mounted");
        return;
    }

    char names[RM_MAX_NAMES][ENFS_NAME_LENGHT];
    UINT32 nameCount = 0;
    bool confirmed = false;
    bool frfr = false;

    while (*args) {
        while (*args == ' ') args++;
        if (!*args) break;

        char token[ENFS_NAME_LENGHT];
        UINT32 n = 0;

        if (*args == '"') {
            args++;
            while (*args && *args != '"' && n < ENFS_NAME_LENGHT - 1) token[n++] = *args++;
            if (*args == '"') args++;
        } else {
            while (*args && *args != ' ' && n < ENFS_NAME_LENGHT - 1) token[n++] = *args++;
        }
        token[n] = '\0';

        if (n == 0) continue;

        if (Str::Equal(token, "--yes")) {
            confirmed = true;
            continue;
        }

        if (Str::Equal(token, "--frfr")) {
            frfr = true;
            continue;
        }

        if (nameCount >= RM_MAX_NAMES) {
            Console::Println("too many files, max 16");
            return;
        }

        for (UINT32 i = 0; i <= n; i++) names[nameCount][i] = token[i];
        nameCount++;
    }

    if (nameCount == 0) {
        Console::Println("usage: rm <name> [<name> ...] --yes [--frfr]");
        return;
    }

    if (!confirmed) {
        Console::Print(frfr ? "will delete and wipe " : "will delete ");
        for (UINT32 i = 0; i < nameCount; i++) {
            Console::Print(names[i]);
            Console::Print(EnFS::Find(names[i]) ? "" : " (not found)");
            if (i + 1 < nameCount) Console::Print(", ");
        }
        Console::Print('\n');
        Console::Println("add --yes to delete");
        return;
    }

    for (UINT32 i = 0; i < nameCount; i++) {
        if (frfr) {
            EnFsEntry *entry = EnFS::Find(names[i]);
            if (entry) {
                UINT32 blockSize = EnFS::super.blockSize;
                UINT64 secs = (entry->byteLenght + blockSize - 1) / blockSize;
                if (secs == 0) secs = 1;

                if (!Part::Zero(entry->startSector, secs)) {
                    Console::Print("rm ");
                    Console::Print(names[i]);
                    Console::Println(": wipe failed");
                    continue;
                }
            }
        }

        EnFsResult r = EnFS::Remove(names[i]);
        Console::Print("rm ");
        Console::Print(names[i]);
        Console::Print(": ");
        Console::Println(EnFS::ResultName(r));
    }
}

static void CmdPlay(const char *args) {
    if (!EnFS::mounted) {
        Console::Println("enfs not mounted");
        return;
    }
    if (!*args) {
        Console::Println("usage: play <script>");
        return;
    }

    if (!scriptBuffer) scriptBuffer = (UINT8*)Heap::Alloc(SCRIPT_MAX_SOURCE, 16);
    if (!scriptBuffer) {
        Console::Println("alloc failed");
        return;
    }

    UINT64 lenght = 0;
    EnFsResult r = EnFS::ReadFile(args, scriptBuffer, SCRIPT_MAX_SOURCE, &lenght);
    if (r != ENFS_OK) {
        Console::Print("play: ");
        Console::Println(EnFS::ResultName(r));
        return;
    }

    INT32 bad = IsEnFile(args) ? Parser::Load((const char*)scriptBuffer, lenght) : Script::Load((const char*)scriptBuffer, lenght);
    if (bad >= 0) {
        Console::Print("bad script line ");
        Console::PrintUInt((UINT64)bad);
        Console::Print('\n');
        return;
    }

    Script::RunLoop(true);
    Console::Println("game ended");
}

static UINT8 *FindGameMagic(UINT8 *blob, UINT64 size) {
    if (size < GAME_HEADER_BYTES + SCRIPT_MAX_SOURCE) return 0;

    for (UINT64 i = 0; i + GAME_HEADER_BYTES + SCRIPT_MAX_SOURCE <= size; i++) {
        bool hit = true;
        for (UINT32 j = 0; j < GAME_MAGIC_LENGHT; j++) {
            if (blob[i + j] != GAME_MAGIC[j]) {
                hit = false;
                break;
            }
        }
        if (hit) return blob + i;
    }
    return 0;
}

static void CmdExport(const char *args) {
    if (!EnFS::mounted) {
        Console::Println("enfs not mounted");
        return;
    }

    char scriptName[ENFS_NAME_LENGHT];
    UINT32 n = 0;
    while (*args && *args != ' ' && n < ENFS_NAME_LENGHT - 1) scriptName[n++] = *args++;
    scriptName[n] = '\0';
    while (*args == ' ') args++;

    if (n == 0 || !*args) {
        Console::Println("usage: export <script> <gamename>");
        return;
    }

    if (!scriptBuffer) scriptBuffer = (UINT8*)Heap::Alloc(SCRIPT_MAX_SOURCE, 16);
    if (!scriptBuffer) {
        Console::Println("alloc failed");
        return;
    }

    UINT64 lenght = 0;
    EnFsResult r = EnFS::ReadFile(scriptName, scriptBuffer, SCRIPT_MAX_SOURCE, &lenght);
    if (r != ENFS_OK) {
        Console::Print("export: ");
        Console::Println(EnFS::ResultName(r));
        return;
    }

    INT32 badLine = Script::Load((const char*)scriptBuffer, lenght);
    if (badLine >= 0) {
        Console::Print("bad script line ");
        Console::PrintUInt((UINT64)badLine);
        Console::Print('\n');
        return;
    }

    UINT64 blobSize = (UINT64)(_binary_RUNTIME_EFI_end - _binary_RUNTIME_EFI_start);

    if (!exportBuffer) exportBuffer = (UINT8*)Heap::Alloc(blobSize, 16);
    if (!exportBuffer) {
        Console::Println("alloc failed");
        return;
    }

    for (UINT64 i = 0; i < blobSize; i++) {
        exportBuffer[i] = _binary_RUNTIME_EFI_start[i];
    }

    UINT8 *slot = FindGameMagic(exportBuffer, blobSize);
    if (!slot) {
        Console::Println("no asset slot in runtime blob");
        return;
    }

    for (UINT32 i = 0; i < 8; i++) {
        slot[GAME_MAGIC_LENGHT + i] = (UINT8)(lenght >> (i * 8));
    }
    for (UINT64 i = 0; i < SCRIPT_MAX_SOURCE; i++) {
        slot[GAME_HEADER_BYTES + i] = i < lenght ? scriptBuffer[i] : 0;
    }

    r = EnFS::WriteFile(args, exportBuffer, blobSize);
    if (r != ENFS_OK) {
        Console::Print("export: ");
        Console::Println(EnFS::ResultName(r));
        return;
    }

    Console::Print("exported ");
    Console::Print(args);
    Console::Print(" ");
    Console::PrintUInt(blobSize);
    Console::Println(" bytes");
}

static void CmdMake(const char *args) {
    if (!EnFS::mounted) {
        Console::Println("EnFS not mounted");
        return;
    }

    char scriptName[ENFS_NAME_LENGHT];
    UINT32 n = 0;
    while (*args && *args != ' ' && n < ENFS_NAME_LENGHT - 1) scriptName[n++] = *args++;
    scriptName[n] = '\0';
    while (*args == ' ') args++;

    if (n == 0 || !*args) {
        Console::Println("usage: make <script.es> <gamename>");
        return;
    }

    if (!scriptBuffer) scriptBuffer = (UINT8*)Heap::Alloc(SCRIPT_MAX_SOURCE);
    if (!buildBuffer) buildBuffer = (UINT8*)Heap::Alloc(ENEXE_MAX_BYTES);
    if (!scriptBuffer || !buildBuffer) {
        Console::Println("alloc failed");
        return;
    }

    UINT64 size = 0;
    EnFsResult res = EnFS::ReadFile(scriptName, scriptBuffer, SCRIPT_MAX_SOURCE, &size);
    if (res != ENFS_OK) {
        Console::Print("make: ");
        Console::Println(EnFS::ResultName(res));
        return;
    }

    INT32 bad = IsEnFile(scriptName) ? Parser::Load((const char*)scriptBuffer, size) : Script::Load((const char*)scriptBuffer, size);
    if (bad >= 0) {
        Console::Print("bad script line ");
        Console::PrintUInt((UINT64)bad);
        Console::Print('\n');
        return;
    }

    UINT64 codeCap = ENEXE_MAX_BYTES - sizeof(EnExeHeader) - Script::stringUsed;
    UINT32 badLine = 0;
    UINT64 codeBytes = Compiler::Compile(buildBuffer + sizeof(EnExeHeader), codeCap, &badLine);

    if (!codeBytes) {
        Console::Print("cannot compile line ");
        Console::PrintUInt((UINT64)badLine);
        Console::Print('\n');
        return;
    }

    UINT64 stringOffset = sizeof(EnExeHeader) + codeBytes;
    for (UINT32 i = 0; i < Script::stringUsed; i++) {
        buildBuffer[stringOffset + i] = (UINT8)Script::stringPool[i];
    }
    UINT64 total = stringOffset + Script::stringUsed;

    EnExeHeader *header = (EnExeHeader*)buildBuffer;
    for (UINT32 i = 0; i < ENEXE_MAGIC_BYTES; i++) {
        header->magic[i] = ENEXE_MAGIC[i];
    }
    header->formatVersion = ENEXE_FORMAT;
    header->ctxVersion = ENEXE_CTX_VERSION;
    header->codeOffset = sizeof(EnExeHeader);
    header->codeBytes = codeBytes;
    header->entryOffset = 0;
    header->stringOffset = stringOffset;
    header->stringBytes = Script::stringUsed;
    header->varCount = Script::varCount;
    header->crc32 = EnExeCrc(buildBuffer, total);

    res = EnFS::WriteFile(args, buildBuffer, total);
    if (res != ENFS_OK) {
        Console::Print("make: ");
        Console::Println(EnFS::ResultName(res));
        return;
    }

    Console::Print("made ");
    Console::Print(args);
    Console::Print(" code ");
    Console::PrintUInt(codeBytes);
    Console::Print(" strings ");
    Console::PrintUInt(Script::stringUsed);
    Console::Print(" total ");
    Console::PrintUInt(total);
    Console::Println(" bytes");
}



const Command Shell::commands[] = {
    {"help", "list commands", CmdHelp},
    {"clear", "clear the terminal", CmdClear},
    {"echo", "print the arguments", CmdEcho},
    {"read", "read <lba> and dump it", CmdRead},
    {"parts", "shows system partitions", CmdGpt},
    {"free", "show unnallocated space", CmdFree},
    {"mkpart", "mkpart <sizeMiB> <name> [--yes]", CmdMkpart},
    {"mount", "mount the engine partition", CmdMount},
    {"partread", "partread <sector> within partition", CmdPartRead},
    {"partwrite", "partwrite <sector> <text>", CmdPartWrite},
    {"wipepart", "wipepart [--yes]. Zero all partitions", CmdWipe},
    {"neenor", "neenor <name>", CmdEditor},
    {"format", "format [--yes] create an enfs volume", CmdFormat},
    {"ls", "list files", CmdLs},
    {"mem", "show memory stats", CmdMem},
    {"rm", "rm <file> [<file> ...] --yes [--frfr], Delete files, frfr wipes data", CmdRm},
    {"play", "play <script.es>, ", CmdPlay},
    {"export", "export <script.es> <gamename>, Link to Runtime.EFI", CmdExport},
    {"make", "make <script.es> <gamename>, compile to machine code", CmdMake},

};

const UINT32 Shell::commandCount = sizeof(commands) / sizeof(commands[0]);
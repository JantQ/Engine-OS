#include "Shell.hh"
#include "../Libs/Str.hh"
#include "../Console/Console.hh"
#include "../PCIe/storage/Nvme.hh"
#include "../Memory/Heap.hh"
#include "../PCIe/storage/Gpt.hh"

static UINT8 *sectorBuffer = 0;

static const UINT8 ENGINE_TYPE_GUID[16] = {
    0x4D, 0x3C, 0x2B, 0x1A,
    0x6F, 0x5E,
    0x7B, 0x4A, 
    0x8C, 0x9D, 0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF4
};

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

const Command Shell::commands[] = {
    {"help", "list commands", CmdHelp},
    {"clear", "clear the terminal", CmdClear},
    {"echo", "print the arguments", CmdEcho},
    {"read", "read <lba> and dump it", CmdRead},
    {"parts", "shows system partitions", CmdGpt},
    {"free", "show unnallocated space", CmdFree},
    {"mkpart", "mkpart <sizeMiB> <name> [--yes]", CmdMkpart},

};

const UINT32 Shell::commandCount = sizeof(commands) / sizeof(commands[0]);
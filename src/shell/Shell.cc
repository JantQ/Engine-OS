#include "Shell.hh"
#include "../Libs/Str.hh"
#include "../Console/Console.hh"
#include "../PCIe/storage/Nvme.hh"
#include "../Memory/Heap.hh"
#include "../PCIe/storage/Gpt.hh"

static UINT8 *sectorBuffer = 0;

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
        Console::Print(entry.endingLba);
        Console::Print(" ");
        Console::PrintUInt(mib);
        Console::Print("M ");
        
        for (UINT32 c = 0; c < 36 && entry.name[c]; c++) {
            Console::Print((char)entry.name[c]);
        }
        Console::Print('\n');
    }
}

const Command Shell::commands[] = {
    {"help", "list commands", CmdHelp},
    {"clear", "clear the terminal", CmdClear},
    {"echo", "print the arguments", CmdEcho},
    {"read", "read <lba> and dump it", CmdRead},
    {"parts", "shows system partitions", CmdGpt},
};

const UINT32 Shell::commandCount = sizeof(commands) / sizeof(commands[0]);
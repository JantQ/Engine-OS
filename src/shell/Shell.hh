#pragma once

#include "../kernel.hh"

typedef void (*CommandFn)(const char *args);

struct Command {
    const char *name;
    const char *help;
    CommandFn handler;
};

static const UINT8 ENGINE_TYPE_GUID[16] = {
    0xA3, 0x7E, 0x1F, 0x92,
    0x4C, 0x6B,
    0x4D, 0x8A,
    0x9F, 0x02, 0x3D, 0x5C, 0x71, 0xE8, 0xB4, 0x6A};

class Shell {
    public:
        static void Execute(const char *line);
        static const Command commands[];
        static const UINT32 commandCount;
};
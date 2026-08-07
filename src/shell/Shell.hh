#pragma once

#include "../kernel.hh"

typedef void (*CommandFn)(const char *args);

struct Command {
    const char *name;
    const char *help;
    CommandFn handler;
};

class Shell {
    public:
        static void Execute(const char *line);
        static const Command commands[];
        static const UINT32 commandCount;
};
#pragma once

#include "../kernel.hh"

class Parser {
    public:
        static INT32 Load(const char *source, UINT64 length);
};
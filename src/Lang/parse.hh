#pragma once

#include "../kernel.hh"

class Parser {
    public:
        static INT32 Load(const char *source, UINT64 length);

        static inline const char *errorMsg = 0;
        static inline UINT32 errorLine = 0;
        static inline UINT32 errorCol = 0;
};
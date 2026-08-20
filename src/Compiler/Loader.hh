#pragma once

#include "Compiler.hh"

enum LoadResult : UINT32{
    LOAD_OK = 0,
    LOAD_TOO_SMALL,
    LOAD_BAD_MAGIC,
    LOAD_BAD_FORMAT,
    LOAD_BAD_CTX,
    LOAD_BAD_HEADER,
    LOAD_BAD_CRC,
    LOAD_NO_PAGES,
};

class Loader {
    public:
        static bool Run(const char *name); // True = a game file
        static LoadResult RunImage(const UINT8 *image, UINT64 size);

        static inline EnCtx ctx;

        static const char *ResultName(LoadResult r) {
            switch(r) {
                case LOAD_OK: return "ok";
                case LOAD_TOO_SMALL: return "TRUNCATED GAME";
                case LOAD_BAD_MAGIC: return "NOT A GAME IMAGE";
                case LOAD_BAD_FORMAT: return "WRONG FORMAT VERSION";
                case LOAD_BAD_CTX: return "WRONG KERNEL VERSION";
                case LOAD_BAD_HEADER: return "BAD HEADER";
                case LOAD_BAD_CRC: return "BAD CRC";
                case LOAD_NO_PAGES: return "NO CODE PAGES";
            }
            return "UNKNOWN";
        }
};
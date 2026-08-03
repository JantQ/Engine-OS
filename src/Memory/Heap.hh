#pragma once

#include "../kernel.hh"

class Heap {
    public: 
        static void Init(EFI_MEMORY_DESCRIPTOR *map, UINTN mapSize, UINTN descriptorSize);
        static void *Alloc(UINTN bytes, UINTN align = 16);
        static void Reset() { offset = 0; }

        static UINTN Used() { return offset; }
        static UINTN Total() { return size; }
        static inline UINT64 totalConv;
        static inline UINT32 convRegions;

    private:
        static inline UINT8 *base;
        static inline UINTN size;
        static inline UINTN offset;
};
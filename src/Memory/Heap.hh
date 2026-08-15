#pragma once

#include "../kernel.hh"
#include "arena.hh"

#define HEAP_RESERVE_BYTES (32 * 1024 * 1024)

class Heap {
    public: 
        static void Init(EFI_MEMORY_DESCRIPTOR *map, UINTN mapSize, UINTN descriptorSize);
        static void *Alloc(UINTN bytes, UINTN align = 16) {
            return permanent.Alloc(bytes, align);
        }
        static void Reset() { return permanent.Reset(); }

        static UINTN Used() { return permanent.Used(); }
        static UINTN Total() { return permanent.Total(); }
        static UINT8 *BaseAddr() { return permanent.BaseAddr(); }
        
        static inline UINT64 totalConv;
        static inline UINT32 convRegions;

    private:
        static inline Arena permanent;
};
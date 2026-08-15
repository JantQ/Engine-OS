#pragma once

#include "../kernel.hh"

#define FRAME_SIZE 4096

class Frames {
    public:
        static UINT64 BitmapBytesNeeded(EFI_MEMORY_DESCRIPTOR *map, UINTN mapSize, UINTN descriptorSize);

        static void Init(EFI_MEMORY_DESCRIPTOR * map, UINTN mapSize, UINTN descriptorSize, void *bitmapMemory, UINT64 bitmapBytes);

        static void *Alloc(UINT64 count, UINT64 align = FRAME_SIZE);
        static void Free(void *addr, UINT64 count);
        static void Reserve(UINT64 addr, UINT64 bytes);

        static inline UINT64 totalFrames = 0;
        static inline UINT64 freeFrames = 0;

    private:
        static inline UINT64 *bitmap = 0;
        static inline UINT64 frameCount = 0;

        static bool TestBit(UINT64 i) {
            return (bitmap[i >> 6] >> (i & 63)) & 1ULL;
        }

        static void SetBit(UINT64 i) {
            bitmap[i >> 6] |= (1ULL << (i & 63));
        }

        static void ClearBit(UINT64 i) {
            bitmap[i >> 6] &= ~(1ULL << (i & 63));
        }
};
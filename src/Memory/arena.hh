#pragma once

#include "../kernel.hh"

class Arena {
    public:
        void Init(void *start, UINTN bytes) {
            base = (UINT8*)start;
            size = bytes;
            offset = 0;
            peak = 0;
        }

        void *Alloc(UINTN bytes, UINTN align = 16) {
            if (bytes == 0 || !base) return 0;
            if (align == 0) align = 1;

            UINT64 addr = (UINT64)base + offset;
            UINT64 aligned = (addr + align - 1) & ~((UINT64)align - 1);
            UINTN newOffset = (UINTN)(aligned - (UINT64)base) + bytes;

            if (newOffset > size) return 0;

            offset = newOffset;
            if (offset > peak) peak = offset;
            return (void*)aligned;
        }

        void Reset() {
            offset = 0;
        }

        UINTN Used() const {
            return offset;
        }

        UINTN Peak() const {
            return peak;
        }

        UINTN Total() const {
            return size;
        }

        UINTN Left() const {
            return size - offset;
        }
        UINT8 *BaseAddr() const {
            return base;
        }


    private:
        UINT8 *base = 0;
        UINTN size = 0;
        UINTN offset = 0;
        UINTN peak = 0;
};
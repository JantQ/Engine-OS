#pragma once

#include "../kernel.hh"

struct Framebuffer {
    UINT32 *pixels;
    UINT32 width;
    UINT32 height;
    UINT32 stride;

    inline void placePixel(UINT32 x, UINT32 y, UINT32 color) {
        if (x >= width || y >= height) return;
        pixels[y * stride + x] = color;
    }

    inline UINT32 getPixel(UINT32 x, UINT32 y) const {
        if (x >= width || y >= height) return 0;
        return pixels[y * stride + x];
    }

    inline void clear(UINT32 color) {
        UINT64 pair = ((UINT64)color << 32) | color;
        UINT64 *p = (UINT64*)pixels;
        UINT64 count = ((UINT64)stride * height) / 2;
        for (UINT64 i = 0; i < count; i++) p[i] = pair;
        UINT64 total = (UINT64)stride * height;
        if (total & 1) pixels[total - 1] = color;
    }

    inline UINTN byteSize() const { return (UINTN)stride * height * sizeof(UINT32);}
};
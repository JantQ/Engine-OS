#pragma once

#include "kernel.hh"

class Text {
    public:
        static void UInt64ToStr(UINT64 value, CHAR16 *buffer);
        static void DrawChar(UINT32 *framebuffer, UINT32 stride, UINT32 x, UINT32 y, char c, UINT32 color);
        static void DrawString(UINT32 *framebuffer, UINT32 stride, UINT32 x, UINT32 y, const char *str, UINT32 color);


        
};
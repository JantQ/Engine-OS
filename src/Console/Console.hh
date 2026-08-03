#pragma once

#include "../kernel.hh"
#include "../Graphics/Framebuffer.hh"
class Console {
    public:
        static const UINT32 MAX_LINE = 32;
        static const UINT32 MAX_COLS = 128;

        static void Print(char c);

        static void Clear();

        static void Draw(Framebuffer &fb, UINT32 x, UINT32 y, UINT32 scale = 1, UINT32 color = 0x00FFFFFF);

        static inline UINT32 lineCount = 1;
        static inline UINT32 col = 0;
    private:
        static void NewLine();

        static inline char lines[MAX_LINE][MAX_COLS];

};
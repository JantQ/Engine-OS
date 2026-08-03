#pragma once

#include <cstdint>
#include "Framebuffer.hh"

class Graphics {
    public:
        static inline Framebuffer front;
        static inline Framebuffer back;

        static uint32_t encodeColor(uint8_t R, uint8_t G, uint8_t B);
        static uint32_t blendColor(uint32_t bgColor, uint32_t fgColor, uint8_t alpha);
    
        static void PresentFrame();

        static void DrawRectangle(Framebuffer &fb, INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color);
    private:

};
#pragma once

#include <cstdint>
#include "../kernel.hh"

class Graphics {
    public:
        static uint32_t encodeColor(uint8_t R, uint8_t G, uint8_t B);
        static uint32_t blendColor(uint32_t bgColor, uint32_t fgColor, uint8_t alpha);
        static void clearBackground(UINT32 *framebuffer, UINT32 width, UINT32 height, UINT32 stride);
    private:

};
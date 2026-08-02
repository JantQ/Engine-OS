#include "graphics.hh"
#include <cstdint>

uint32_t Graphics::encodeColor(uint8_t R, uint8_t G, uint8_t B) {
    return (static_cast<uint32_t>(R) << 16) |
           (static_cast<uint32_t>(G) << 8) |
           (static_cast<uint32_t>(B));
}


uint32_t Graphics::blendColor(uint32_t bgColor, uint32_t fgColor, uint8_t alpha) {
    uint8_t bgR = (bgColor >> 16) & 0xFF, bgG = (bgColor >> 8) & 0xFF, bgB = bgColor & 0xFF;
    uint8_t fgR = (fgColor >> 16) & 0xFF, fgG = (fgColor >> 8) & 0xFF, fgB = fgColor & 0xFF;

    uint8_t r = (fgR * alpha + bgR * (255 - alpha)) / 255;
    uint8_t g = (fgG * alpha + bgG * (255 - alpha)) / 255;
    uint8_t b = (fgB * alpha + bgB * (255 - alpha)) / 255;

    return (r << 16) | (g << 8) | b;
}

void Graphics::clearBackground(UINT32 *framebuffer, UINT32 width, UINT32 height, UINT32 stride) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            framebuffer[y * stride + x] = 0x00000000;
        }
    }
}
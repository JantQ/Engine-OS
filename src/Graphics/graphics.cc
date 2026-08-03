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

void Graphics::PresentFrame() {
    UINT64 *dst = (UINT64*)front.pixels;
    const UINT64 *src = (const UINT64*)back.pixels;
    UINT64 total = (UINT64)front.stride * front.height;
    UINT64 count = total / 2;

    for (UINT64 i = 0; i < count; i++) {
        __asm__ __volatile__("movnti %1, %0" : "=m"(dst[i]) : "r"(src[i]));
    }
    if (total & 1) front.pixels[total - 1] = back.pixels[total - 1];
    __asm__ __volatile__("sfence" ::: "memory");
}

void Graphics::DrawRectangle(Framebuffer &fb, INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color) {
    INT32 x0 = x < 0 ? 0 : x;
    INT32 y0 = y < 0 ? 0 : y;
    INT32 x1 = x + w > (INT32)fb.width  ? (INT32)fb.width  : x + w;
    INT32 y1 = y + h > (INT32)fb.height ? (INT32)fb.height : y + h;

    for (INT32 py = y0; py < y1; py++) {
        UINT32 *row = fb.pixels + (UINT32)py * fb.stride;
        for (INT32 px = x0; px < x1; px++) row[px] = color;
    }
}

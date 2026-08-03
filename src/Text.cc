#include "Text.hh"
#include "Graphics/Framebuffer.hh"
#include "Graphics/graphics.hh"
#include "kernel.hh"

void Text::UInt64ToStr(UINT64 value, CHAR16 *buffer) {
    CHAR16 temp[21];
    int i = 0;

    if (value == 0) {
        buffer[0] = L'0';
        buffer[1] = L'\0';
        return;
    }

    while (value > 0) {
        temp[i++] = L'0' + (value % 10);
        value /= 10;
    }

    int j = 0;

    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = L'\0';
}




void Text::DrawChar(Framebuffer &fb, UINT32 x, UINT32 y, char c, UINT32 scale, UINT32 color) {
    const UINT8 *glyph = GetGlyph(c);
    for (UINT32 row = 0; row < 8; row++) {
        UINT8 bits = glyph[row];
        for (UINT32 col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                for (UINT32 dy = 0; dy < scale; dy++) {
                    for (UINT32 dx = 0; dx < scale; dx++) {
                        fb.placePixel(x + col * scale + dx, y + row * scale + dy, color);
                    }
                }
            }
        }
    }
}

void Text::DrawString(Framebuffer &fb, UINT32 x, UINT32 y, const char *str, UINT32 scale, UINT32 color) {
    UINT32 cursorX = x;
    while (*str) {
        DrawChar(fb, cursorX, y, *str, scale, color);
        cursorX += 9 * scale; 
        str++;
    }
}

void Text::DrawLogo(Framebuffer &fb, UINT32 x, UINT32 y, UINT32 scale, UINT32 color) {
    const char *lines[] = {
"   (\'-.       .-\') _                         .-\') _   (\'-.                       .-\')    ",
" _(  OO)     ( OO ) )                       ( OO ) )_(  OO)                     ( OO ).  ",
"(,------.,--./ ,--,\'  ,----.     ,-.-\') ,--./ ,--,\'(,------.       .-\'),-----. (_)---\\_) ",
" |  .---\'|   \\ |  |\\ \'  .-./-\')  |  |OO)|   \\ |  |\\ |  .---\'      ( OO\'  .-.  \'/    _ |  ",
" |  |    |    \\|  | )|  |_( O- ) |  |  \\|    \\|  | )|  |          /   |  | |  |\\  :` `.  ",
"(|  \'--. |  .     |/ |  | .--, \\ |  |(_/|  .     |/(|  \'--.       \\_) |  |\\|  | \'..`\'\'.) ",
" |  .--\' |  |\\    | (|  | \'. (_/,|  |_.\'|  |\\    |  |  .--\'         \\ |  | |  |.-._)   \\ ",
" |  `---.|  | \\   |  |  \'--\'  |(_|  |   |  | \\   |  |  `---.         `\'  \'-\'  \'\\       / ",
 "`------\'`--\'  `--\'   `------\'   `--\'   `--\'  `--\'  `------\'           `-----\'  `-----\' ",
    };

    UINT32 lineHeight = 9 * scale;

    for (UINT32 i = 0; i < 9; i++) {
        DrawString(fb, x, y + i * lineHeight, lines[i], scale, color);
    }
}


void Text::DrawUInt(Framebuffer &fb, UINT32 x, UINT32 y, UINT64 value, UINT32 scale, UINT32 color) {
    CHAR16 buf[21];
    UInt64ToStr(value, buf);
    for (int i = 0; buf[i] != '\0'; i++) {
        DrawChar(fb, x + i * 8 * scale, y, buf[i], scale, color);
    }
}

static void Print(char string) {
    
}
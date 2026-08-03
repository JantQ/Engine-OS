#include "Console.hh"
#include "../Text.hh"

void Console::NewLine() {
    col = 0;

    if (lineCount < MAX_LINE) {
        lineCount++;
        return;
    }

    for (UINT32 i = 1; i < MAX_LINE; i++) {
        for (UINT32 j = 0; j < MAX_COLS; j++) {
            lines[i - 1][j] = lines[i][j];
        }
    }
    for (UINT32 j = 0; j < MAX_COLS; j++) {
        lines[MAX_LINE - 1][j] = '\0';
    }
}

void Console::Print(char c) {
    if (c == '\n') {
        NewLine();
        return;
    }

    if (c == '\b') {
        if (col > 0) {
            lines[lineCount - 1][--col] = '\0';
        }
        return;
    }

    if (col >= MAX_COLS - 1) NewLine();

    lines[lineCount - 1][col++] = c;
    lines[lineCount - 1][col] = '\0';
}

void Console::Clear() {
    for (UINT32 i = 0; i < MAX_LINE; i++) {
        lines[i][0] = '\0';
    }
    lineCount = 1;
    col = 0;
}

void Console::Draw(Framebuffer &fb, UINT32 x, UINT32 y, UINT32 scale, UINT32 color) {
    UINT32 lineHeight = 9 * scale;
    for (UINT32 i = 0; i < lineCount; i++) {
        Text::DrawString(fb, x, y + i * lineHeight, lines[i], scale, color);
    }
}
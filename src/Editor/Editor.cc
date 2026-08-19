#include "Editor.hh"
#include "../Text.hh"
#include "../FileSystem/EnFS.hh"
#include "../Memory/Heap.hh"
#include "../input/keyboard.hh"

#define ED_BUFFER_BYTES (ED_MAX_LINES * ED_MAX_COLS)

static UINT8 *fileBuffer = 0;

static bool EnsureBuffer() {
    if (!fileBuffer) fileBuffer = (UINT8*)Heap::Alloc(ED_BUFFER_BYTES, 16);
    return fileBuffer != 0;
}

void Editor::Open(const char *name) {
    for (UINT32 i = 0; i < ED_MAX_LINES; i++) lengths[i] = 0;
    lineCount = 1;
    curLine = 0;
    curCol = 0;
    scrollTop = 0;
    dirty = false;

    UINT32 n = 0;
    while (name[n] && n < 39) {
        fileName[n] = name[n];
        n++;
    }
    fileName[n] = '\0';

    if (EnsureBuffer()) {
        UINT64 lentgh = 0;
        if (EnFS::ReadFile(fileName, fileBuffer, ED_BUFFER_BYTES, &lentgh) == ENFS_OK) {
            UINT32 line = 0;
            UINT32 col = 0;

            for (UINT64 i = 0; i < lentgh; i++) {
                char c = (char)fileBuffer[i];

                if (c == '\r') continue;
                if (c == '\n') {
                    lengths[line] = col;
                    line++;
                    col = 0;
                    if (line >= ED_MAX_LINES) {
                        line = ED_MAX_LINES - 1;
                        break;
                    }
                    continue;
                }
                if (col < ED_MAX_COLS - 1) {
                    lines[line][col++] = c;
                }
            }
            lengths[line] = col;
            lineCount = line + 1;
        } 
    }

    active = true;
}

bool Editor::Save() {
    if (!EnsureBuffer()) return false;

    UINT64 total = 0;
    for (UINT32 l = 0; l < lineCount; l++) {
        for (UINT32 i = 0; i < lengths[l]; i++) {
            fileBuffer[total++] = (UINT8)lines[l][i];
        }
        if (l + 1 < lineCount) {
            fileBuffer[total++] = (UINT8)'\n';
        }
    }

    if (EnFS::WriteFile(fileName, fileBuffer, total) != ENFS_OK) return false;

    dirty = false;
    return true;
}

void Editor::ClampCursor() {
    if (curCol > lengths[curLine]) curCol = lengths[curLine];
}

void Editor::ScrollToCursor() {
    if (curLine < scrollTop) scrollTop = curLine;
    if (curLine >= scrollTop + ED_VISIBLE) scrollTop = curLine - ED_VISIBLE + 1;
}

bool Editor::InsertChar(char c) {
    UINT32 lenght = lengths[curLine];
    if (lenght >= ED_MAX_COLS - 1) return false;

    for (UINT32 i = lenght; i > curCol; i--) {
        lines[curLine][i] = lines[curLine][i - 1];
    }
    lines[curLine][curCol] = c;

    lengths[curLine] = lenght + 1;
    curCol++;
    dirty = true;
    return true;
}

void Editor::JoinLine() {
    UINT32 prev = curLine - 1;
    UINT32 plen = lengths[prev];
    UINT32 room = ED_MAX_COLS - 1 - plen;
    UINT32 n = lengths[curLine] < room ? lengths[curLine] : room;

    for (UINT32 i = 0; i < n; i++) {
        lines[prev][plen + i] = lines[curLine][i];
    }
    lengths[prev] = plen + n;

    for (UINT32 l = curLine; l + 1 < lineCount; l++) {
        for (UINT32 i = 0; i < lengths[l + 1]; i++) {
            lines[l][i] = lines[l + 1][i];
        }
        lengths[l] = lengths[l + 1];
    }

    lineCount--;
    curLine = prev;
    curCol = plen;
    dirty = true;
}

void Editor::BackSpace() {
    if (curCol > 0) {
        UINT32 lenght = lengths[curLine];
        for (UINT32 i = curCol - 1; i + 1 < lenght; i++) {
            lines[curLine][i] = lines[curLine][i + 1];
        }
        lengths[curLine] = lenght - 1;
        curCol--;
        dirty = true;
        return;
    }
    if (curLine > 0) JoinLine();
}

void Editor::SplitLine() {
    if (lineCount >= ED_MAX_LINES) return;

    for (UINT32 l = lineCount; l > curLine + 1; l--) {
        for (UINT32 i = 0; i < lengths[l - 1]; i++) {
            lines[l][i] = lines[l - 1][i];
        }
        lengths[l] = lengths[l - 1];
    }

    UINT32 tail = lengths[curLine] - curCol;
    for (UINT32 i = 0; i < tail; i++) {
        lines[curLine + 1][i] = lines[curLine][curCol + i];
    }

    lengths[curLine + 1] = tail;
    lengths[curLine] = curCol;

    lineCount++;
    curLine++;
    curCol = 0;
    dirty = true;
}

void Editor::Update(UINT16 key) {
    if (key == 0) return;

    if (key == KEY_CTRL_Q || key == KEY_ESC) {
        active = false;
        return;
    }
    if (key == KEY_CTRL_S) {
        Save();
        return;
    }

    if (key == KEY_LEFT) {
        if (curCol > 0) curCol--;
        else if (curLine > 0) { curLine--; curCol = lengths[curLine]; }
    } else if (key == KEY_RIGHT) {
        if (curCol < lengths[curLine]) curCol++;
        else if (curLine + 1 < lineCount) { curLine++; curCol = 0; }
    } else if (key == KEY_UP) {
        if (curLine > 0) { curLine--; ClampCursor(); }
    } else if (key == KEY_DOWN) {
        if (curLine + 1 < lineCount) {curLine++; ClampCursor(); }
    } else if (key == KEY_HOME) {
        curCol = 0;
    } else if (key == KEY_END) {
        curCol = lengths[curLine];
    } else if (key == KEY_DELETE) {
        UINT32 lentgh = lengths[curLine];
        if (curCol < lentgh) {
            for (UINT32 i = curCol; i < lentgh; i++) {
                lines[curLine][i] = lines[curLine][i + 1];
            }
            lengths[curLine] = lentgh - 1;
            dirty = true;
        } else if (curLine + 1 < lineCount) {
            curLine++;
            JoinLine();
        }
    } else if (key < 0x100) {
        char c = (char)key;
        if (c == '\n') SplitLine();
        else if (c == '\b') BackSpace();
        else if (c == '\t') {
            for (UINT32 i = 0; i < ED_TAB_WIDTH; i++) InsertChar(' ');
        }
        else if (c == '{' || c == '(') {
            char close = (c == '{') ? '}' : ')';
            if (InsertChar(c) && InsertChar(close)) curCol--;
        }
        else if (c == '}' || c == ')') {
            if (curCol < lengths[curLine] && lines[curLine][curCol] == c) curCol++;
            else InsertChar(c);
        }
        else if (c >= 32) InsertChar(c);
    }

    ScrollToCursor();
}

void Editor::Draw(Framebuffer &fb) {
    UINT32 y = 0;

    for (UINT32 r = 0; r < ED_VISIBLE; r++) {
        UINT32 l = scrollTop + r;
        if (l >= lineCount) break;

        lines[l][lengths[l]] = '\0';
        Text::DrawString(fb, 0, y, lines[l], ED_SCALE);
        y += ED_LINE_H;
    }

    UINT32 cy = (curLine - scrollTop) * ED_LINE_H;
    Text::DrawChar(fb, curCol * ED_CHAR_W, cy, '_', ED_SCALE);

    UINT32 sy = fb.height - ED_LINE_H;
    Text::DrawString(fb, 0, sy, dirty ? "*" : " ", ED_SCALE);
    Text::DrawString(fb, ED_CHAR_W * 2, sy, fileName, ED_SCALE);
    Text::DrawUInt(fb, ED_CHAR_W * 44, sy, curLine + 1);
    Text::DrawString(fb, ED_CHAR_W * 50, sy, ":", ED_SCALE);
    Text::DrawUInt(fb, ED_CHAR_W * 52, sy, curCol + 1);
    Text::DrawString(fb, ED_CHAR_W * 60, sy, "^S save  ^Q quit", ED_SCALE);
}
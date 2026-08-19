#pragma once

#include "../kernel.hh"
#include "../Graphics/Framebuffer.hh"

#define ED_SCALE 2
#define ED_LINE_H (9 * ED_SCALE)
#define ED_CHAR_W (9 * ED_SCALE)
#define ED_VISIBLE 50
#define ED_MAX_LINES 256
#define ED_MAX_COLS 120
#define ED_TAB_WIDTH 3

class Editor {
    public:
        static inline bool active = false;
        static inline bool dirty = false;

        static void Open(const char *name);
        static void Update(UINT16 key);
        static void Draw(Framebuffer &fb);

        static bool Save();

    private:
        static inline char lines[ED_MAX_LINES][ED_MAX_COLS];
        static inline UINT32 lengths[ED_MAX_LINES];
        static inline UINT32 lineCount = 1;
        static inline UINT32 curLine = 0;
        static inline UINT32 curCol = 0;
        static inline UINT32 scrollTop = 0;
        static inline char fileName[40];

        static bool InsertChar(char c);
        static void BackSpace();
        static void SplitLine();
        static void JoinLine();
        static void ClampCursor();
        static void ScrollToCursor();
};
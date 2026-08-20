#pragma once

#include "../kernel.hh"

#define SCRIPT_MAX_SOURCE 32768
#define SCRIPT_MAX_LINES 512
#define SCRIPT_MAX_VARS 64
#define SCRIPT_MAX_LABLES 64
#define SCRIPT_STRING_POOL 2048
#define SCRIPT_NAME_LENGHT 16
#define SCRIPT_ARENA_BYTES (256 * 1024)
#define SCRIPT_CALL_DEPTH 32

#define GAME_MAGIC_LENGHT 16
#define GAME_HEADER_BYTES (GAME_MAGIC_LENGHT + 8)

static const UINT8 GAME_MAGIC[GAME_MAGIC_LENGHT] = {
    'E','n','G','i','N','e','G','a','M','e','B','l','O','b','2','!'};

enum ScriptOp : UINT8 {
    OP_NONE = 0,
    OP_SET,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_RND,
    OP_GOTO,
    OP_IF,
    OP_RECT,
    OP_TEXT,
    OP_NUM,
    OP_KEY,
    OP_SCREEN,
    OP_MILLIS,
    OP_WAIT,
    OP_FLIP,
    OP_END,
    OP_ALLOC,
    OP_POKE,
    OP_PEEK,
    OP_GOSUB,
    OP_RETURN,
};

struct ScriptArg {
    bool isVar;
    INT64 value;
};

struct ScriptLine {
    UINT8 op;
    char cmp;
    UINT8 neg;
    ScriptArg a, b, c, d, e;
    INT32 target;
    UINT32 strOffset;
    UINT32 srcLine;
    const char *ref;
    UINT32 refLenght;
};

struct Token {
    const char *start;
    UINT32 lenght;
};

class Script {
    public:
        static INT32 Load(const char *source, UINT64 lenght);
        static bool Step();
        static void RunLoop(bool allowEsc);
        static void FreeArena();

        static void ResetState();
        static bool ParseStatement(Token *token, UINT32 n, UINT32 srcLine);
        static INT32 ResolveLables();

        static bool ParseArg(Token &t, ScriptArg *out);
        static bool ParseVar(Token &t, ScriptArg *out);

        struct Lable {
            char name[SCRIPT_NAME_LENGHT];
            UINT32 line;
        };

        static inline UINT32 lineCount = 0;
        static inline UINT32 pc = 0;

        static inline ScriptLine lines[SCRIPT_MAX_LINES];
        static inline char varNames[SCRIPT_MAX_VARS][SCRIPT_NAME_LENGHT];
        static inline INT64 vars[SCRIPT_MAX_VARS];
        static inline UINT32 varCount = 0;
        static inline Lable lables[SCRIPT_MAX_LABLES];
        static inline UINT32 lableCount = 0;
        static inline char stringPool[SCRIPT_STRING_POOL];
        static inline UINT32 stringUsed = 0;
        static inline UINT64 rngState = 1;

        static inline INT64 *arena = 0;
        static inline UINT64 arenaSlots = 0;
        static inline UINT64 arenaUsed = 0;

        static inline UINT32 callStack[SCRIPT_CALL_DEPTH];
        static inline UINT32 callDepth = 0;

    private:
        static INT64 Val(ScriptArg &arg);
        static UINT64 Rand();
};

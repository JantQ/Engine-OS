#include "Script.hh"
#include "../Graphics/graphics.hh"
#include "../Text.hh"
#include "../Clock/Clock.hh"
#include "../input/keyboard.hh"
#include "../Memory/Framse.hh"

static bool TokenIs(Token &t, const char *word) {
    UINT32 i = 0;
    while (i < t.lenght && word[i]) {
        if (t.start[i] != word[i]) return false;
        i++;
    }
    return i == t.lenght && word[i] == '\0';
}

static UINT32 Tokenize(const char *p, const char *end, Token *toks, UINT32 max) {
    UINT32 count = 0;

    while (p < end && count < max) {
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p >= end) break;

        if (*p == '"') {
            p++;
            toks[count].start = p;
            while (p < end && *p != '"') p++;
            toks[count].lenght = (UINT32)(p - toks[count].start);
            if (p < end) p++;
        } else {
            toks[count].start = p;
            while (p < end && *p != ' ' && *p != '\t') p++;
            toks[count].lenght = (UINT32)(p - toks[count].start);
        }
        count++;
    }
    return count;
}

static INT32 FindVarSlot(Token &t) {
    if (t.lenght == 0 || t.lenght >= SCRIPT_NAME_LENGHT) return -1;

    for (UINT32 i = 0; i < Script::varCount; i++) {
        UINT32 j = 0;
        while (j < t.lenght && Script::varNames[i][j] == t.start[j]) j++;
        if (j == t.lenght && Script::varNames[i][j] == '\0') return (INT32)i;
    }

    if (Script::varCount >= SCRIPT_MAX_VARS) return -1;

    UINT32 slot = Script::varCount++;
    for (UINT32 j = 0; j < t.lenght; j++) Script::varNames[slot][j] = t.start[j];
    Script::varNames[slot][t.lenght] = '\0';
    Script::vars[slot] = 0;
    return (INT32)slot;
}

bool Script::ParseArg(Token &t, ScriptArg *out) {
    if (t.lenght == 0) return false;

    const char *p = t.start;
    bool negative = false;
    UINT32 i = 0;

    if (p[0] == '-' && t.lenght > 1) {
        negative = true;
        i = 1;
    }

    if (t.lenght > i + 2 && p[i] == '0' && (p[i + 1] == 'x' || p[i + 1] == 'X')) {
        INT64 v = 0;
        for (UINT32 k = i + 2; k < t.lenght; k++) {
            char c = p[k];
            INT64 d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            v = v * 16 + d;
        }
        out->isVar = false;
        out->value = negative ? -v : v;
        return true;
    }

    if (p[i] >= '0' && p[i] <= '9') {
        INT64 v = 0;
        for (UINT32 k = i; k < t.lenght; k++) {
            if (p[k] < '0' || p[k] > '9') return false;
            v = v * 10 + (p[k] - '0');
        }
        out->isVar = false;
        out->value = negative ? -v : v;
        return true;
    }

    INT32 slot = FindVarSlot(t);
    if (slot < 0) return false;
    out->isVar = true;
    out->value = slot;
    return true;
}

bool Script::ParseVar(Token &t, ScriptArg *out) {
    if (t.lenght == 0) return false;
    if (t.start[0] >= '0' && t.start[0] <= '9') return false;

    INT32 slot = FindVarSlot(t);
    if (slot < 0) return false;
    out->isVar = true;
    out->value = slot;
    return true;
}

static bool ParseKeyName(Token &t, ScriptArg *out) {
    out->isVar = false;

    if (TokenIs(t, "up")) { out->value = KEY_UP; return true; }
    if (TokenIs(t, "down")) { out->value = KEY_DOWN; return true; }
    if (TokenIs(t, "left")) { out->value = KEY_LEFT; return true; }
    if (TokenIs(t, "right")) { out->value = KEY_RIGHT; return true; }
    if (TokenIs(t, "space")) { out->value = ' '; return true; }
    if (TokenIs(t, "enter")) { out->value = '\n'; return true; }
    if (TokenIs(t, "esc")) { out->value = KEY_ESC; return true; }

    if (t.lenght == 1) {
        out->value = (UINT8)t.start[0];
        return true;
    }
    return false;
}

void Script::ResetState() {
    FreeArena();
    lineCount = 0;
    pc = 0;
    varCount = 0;
    lableCount = 0;
    stringUsed = 0;
    callDepth = 0;
    rngState = Clock::rdtsc() | 1;
}

bool Script::ParseStatement(Token *toks, UINT32 n, UINT32 srcLine) {
    if (TokenIs(toks[0], "label")) {
        if (n != 2 || toks[1].lenght >= SCRIPT_NAME_LENGHT) return false;
        if (lableCount >= SCRIPT_MAX_LABLES) return false;

        Lable &lb = lables[lableCount++];

        for (UINT32 j = 0; j < toks[1].lenght; j++) {
            lb.name[j] = toks[1].start[j];
        }
        lb.name[toks[1].lenght] = '\0';
        lb.line = lineCount;
        return true;
    }

    if (lineCount >= SCRIPT_MAX_LINES) return false;

    ScriptLine &L = lines[lineCount];
    L.op = OP_NONE;
    L.cmp = 0;
    L.neg = 0;
    L.target = -1;
    L.srcLine = srcLine;
    L.ref = 0;
    L.refLenght = 0;

    if (TokenIs(toks[0], "set") || TokenIs(toks[0], "add") || TokenIs(toks[0], "sub")
        || TokenIs(toks[0], "mul") || TokenIs(toks[0], "div") || TokenIs(toks[0], "rnd")) {
        if (n != 3) return false;
        if (TokenIs(toks[0], "set")) L.op = OP_SET;
        else if (TokenIs(toks[0], "add")) L.op = OP_ADD;
        else if (TokenIs(toks[0], "sub")) L.op = OP_SUB;
        else if (TokenIs(toks[0], "mul")) L.op = OP_MUL;
        else if (TokenIs(toks[0], "div")) L.op = OP_DIV;
        else L.op = OP_RND;

        if (!ParseVar(toks[1], &L.a)) return false;
        if (!ParseArg(toks[2], &L.b)) return false;
    }
    else if (TokenIs(toks[0], "goto")) {
        if (n != 2) return false;
        L.op = OP_GOTO;
        L.ref = toks[1].start;
        L.refLenght = toks[1].lenght;
    }
    else if (TokenIs(toks[0], "gosub")) {
        if (n != 2) return false;
        L.op = OP_GOSUB;
        L.ref = toks[1].start;
        L.refLenght = toks[1].lenght;
    }
    else if (TokenIs(toks[0], "return")) {
        if (n != 1) return false;
        L.op = OP_RETURN;
    }
    else if (TokenIs(toks[0], "if")) {
        if (n != 6 || toks[2].lenght != 1) return false;
        if (!TokenIs(toks[4], "goto")) return false;

        char c = toks[2].start[0];
        if (c != '=' && c != '!' && c != '<' && c != '>') return false;

        L.op = OP_IF;
        L.cmp = c;
        if (!ParseArg(toks[1], &L.a)) return false;
        if (!ParseArg(toks[3], &L.b)) return false;
        L.ref = toks[5].start;
        L.refLenght = toks[5].lenght;
    }
    else if (TokenIs(toks[0], "rect")) {
        if (n != 6) return false;
        L.op = OP_RECT;
        if (!ParseArg(toks[1], &L.a)) return false;
        if (!ParseArg(toks[2], &L.b)) return false;
        if (!ParseArg(toks[3], &L.c)) return false;
        if (!ParseArg(toks[4], &L.d)) return false;
        if (!ParseArg(toks[5], &L.e)) return false;
    }
    else if (TokenIs(toks[0], "text")) {
        if (n != 4) return false;
        if (Script::stringUsed + toks[3].lenght + 1 > SCRIPT_STRING_POOL) return false;
        L.op = OP_TEXT;
        if (!ParseArg(toks[1], &L.a)) return false;
        if (!ParseArg(toks[2], &L.b)) return false;
        L.strOffset = Script::stringUsed;
        for (UINT32 j = 0; j < toks[3].lenght; j++) Script::stringPool[Script::stringUsed++] = toks[3].start[j];
        Script::stringPool[Script::stringUsed++] = '\0';
    }
    else if (TokenIs(toks[0], "num")) {
        if (n != 4) return false;
        L.op = OP_NUM;
        if (!ParseArg(toks[1], &L.a)) return false;
        if (!ParseArg(toks[2], &L.b)) return false;
        if (!ParseArg(toks[3], &L.c)) return false;
    }
    else if (TokenIs(toks[0], "key")) {
        if (n != 3) return false;
        L.op = OP_KEY;
        if (!ParseKeyName(toks[1], &L.b)) return false;
        if (!ParseVar(toks[2], &L.a)) return false;
    }
    else if (TokenIs(toks[0], "screen")) {
        if (n != 3) return false;
        L.op = OP_SCREEN;
        if (!ParseVar(toks[1], &L.a)) return false;
        if (!ParseVar(toks[2], &L.b)) return false;
    }
    else if (TokenIs(toks[0], "millis")) {
        if (n != 2) return false;
        L.op = OP_MILLIS;
        if (!ParseVar(toks[1], &L.a)) return false;
    }
    else if (TokenIs(toks[0], "wait")) {
        if (n != 2) return false;
        L.op = OP_WAIT;
        if (!ParseArg(toks[1], &L.a)) return false;
    }
    else if (TokenIs(toks[0], "alloc")) {
        if (n != 3) return false;
        L.op = OP_ALLOC;
        if (!ParseVar(toks[1], &L.a)) return false;
        if (!ParseArg(toks[2], &L.b)) return false;
    }
    else if (TokenIs(toks[0], "poke")) {
        if (n != 4) return false;
        L.op = OP_POKE;
        if (!ParseArg(toks[1], &L.a)) return false;
        if (!ParseArg(toks[2], &L.b)) return false;
        if (!ParseArg(toks[3], &L.c)) return false;
    }
    else if (TokenIs(toks[0], "peek")) {
        if (n != 4) return false;
        L.op = OP_PEEK;
        if (!ParseArg(toks[1], &L.a)) return false;
        if (!ParseArg(toks[2], &L.b)) return false;
        if (!ParseVar(toks[3], &L.c)) return false;
    }
    else if (TokenIs(toks[0], "flip")) {
        L.op = OP_FLIP;
    }
    else if (TokenIs(toks[0], "end")) {
        L.op = OP_END;
    }
    else {
        return false;
    }

    lineCount++;
    return true;
}

INT32 Script::ResolveLables() {
    for (UINT32 i = 0; i < lineCount; i++) {
        ScriptLine &L = Script::lines[i];
        if (!L.ref) continue;

        L.target = -1;
        for (UINT32 lb = 0; lb < Script::lableCount; lb++) {
            UINT32 j = 0;
            while (j < L.refLenght && Script::lables[lb].name[j] == L.ref[j]) j++;
            if (j == L.refLenght && Script::lables[lb].name[j] == '\0') {
                L.target = (INT32)Script::lables[lb].line;
                break;
            }
        }
        if (L.target < 0) return (INT32)L.srcLine;
    }

    return -1;
}

INT64 Script::Val(ScriptArg &arg) {
    return arg.isVar ? Script::vars[arg.value] : arg.value;
}

UINT64 Script::Rand() {
    Script::rngState ^= Script::rngState << 13;
    Script::rngState ^= Script::rngState >> 7;
    Script::rngState ^= Script::rngState << 17;
    return Script::rngState;
}

INT32 Script::Load(const char *source, UINT64 lenght) {
    ResetState();

    const char *p = source;
    const char *end = source + lenght;
    INT32 srcLine = 0;
    bool inComment = false;

    while (p < end) {
        srcLine++;

        const char *lineEnd = p;
        while (lineEnd < end && *lineEnd != '\n') lineEnd++;

        if (inComment) {
            for (const char *s = p; s + 1 < lineEnd; s++) {
                if (s[0] == 'C' && s[1] == '#') {
                    inComment = false;
                    break;
                }
            }
            p = lineEnd < end ? lineEnd + 1 : end;
            continue;
        }

        const char *cut = lineEnd;
        bool inQuote = false;
        for (const char *s = p; s + 1 < lineEnd; s++) {
            if (*s == '"') inQuote = !inQuote;
            if (inQuote) continue;
            if (s[0] == '#' && s[1] == 'C') {
                cut = s;
                break;
            }
            if (s[0] == 'C' && s[1] == '#') {
                cut = s;
                inComment = true;

                for (const char *c = s + 2; c + 1 < lineEnd; c++) {
                    if (c[0] == 'C' && c[1] == '#') {
                        inComment = false;
                        break;
                    }
                }
                break;
            }
        }

        Token toks[8];
        UINT32 n = Tokenize(p, cut, toks, 8);
        p = lineEnd < end ? lineEnd + 1 : end;

        if (n == 0) continue;
        
        if (!ParseStatement(toks, n, (UINT32)srcLine)) return srcLine;
    }

    return ResolveLables();
}

bool Script::Step() {
    UINT32 executed = 0;

    while (pc < lineCount) {
        if (++executed > 200000) return true;

        ScriptLine &L = Script::lines[pc];

        switch (L.op) {
            case OP_SET: Script::vars[L.a.value] = Val(L.b); pc++; break;
            case OP_ADD: Script::vars[L.a.value] += Val(L.b); pc++; break;
            case OP_SUB: Script::vars[L.a.value] -= Val(L.b); pc++; break;
            case OP_MUL: Script::vars[L.a.value] *= Val(L.b); pc++; break;
            case OP_DIV: {
                INT64 d = Val(L.b);
                Script::vars[L.a.value] = d != 0 ? Script::vars[L.a.value] / d : 0;
                pc++;
                break;
            }
            case OP_RND: {
                INT64 max = Val(L.b);
                Script::vars[L.a.value] = max > 0 ? (INT64)(Rand() % (UINT64)max) : 0;
                pc++;
                break;
            }
            case OP_GOTO: pc = (UINT32)L.target; break;
            case OP_GOSUB:
                if (callDepth >= SCRIPT_CALL_DEPTH) return false;
                callStack[callDepth++] = pc + 1;
                pc = (UINT32)L.target;
                break;
            case OP_RETURN:
                if (callDepth == 0) return false;
                pc = callStack[--callDepth];
                break;
            case OP_IF: {
                INT64 x = Val(L.a);
                INT64 y = Val(L.b);
                bool hit = false;
                if (L.cmp == '=') hit = x == y;
                if (L.cmp == '!') hit = x != y;
                if (L.cmp == '<') hit = x < y;
                if (L.cmp == '>') hit = x > y;
                if (L.cmp == 'l') hit = x <= y;
                if (L.cmp == 'g') hit = x >= y;
                pc = (hit != (bool)L.neg)? (UINT32)L.target : pc + 1;
                break;
            }
            case OP_RECT:
                Graphics::DrawRectangle(Graphics::back, (INT32)Val(L.a), (INT32)Val(L.b),
                    (INT32)Val(L.c), (INT32)Val(L.d), (UINT32)Val(L.e));
                pc++;
                break;
            case OP_TEXT:
                Text::DrawString(Graphics::back, (UINT32)Val(L.a), (UINT32)Val(L.b),
                    Script::stringPool + L.strOffset, 2);
                pc++;
                break;
            case OP_NUM:
                Text::DrawUInt(Graphics::back, (UINT32)Val(L.a), (UINT32)Val(L.b), (UINT64)Val(L.c), 2);
                pc++;
                break;
            case OP_KEY:
                Script::vars[L.a.value] = Keyboard::IsDown((UINT16)L.b.value) ? 1 : 0;
                pc++;
                break;
            case OP_SCREEN:
                Script::vars[L.a.value] = Graphics::back.width;
                Script::vars[L.b.value] = Graphics::back.height;
                pc++;
                break;
            case OP_MILLIS:
                Script::vars[L.a.value] = (INT64)Clock::Millis();
                pc++;
                break;
            case OP_WAIT: {
                UINT64 untill = Clock::Millis() + (UINT64)Val(L.a);
                while (Clock::Millis() < untill) Keyboard::PumpState();
                pc++;
                break;
            }
            case OP_ALLOC: {
                if (!arena) {
                    arena = (INT64*)Frames::Alloc(SCRIPT_ARENA_BYTES / FRAME_SIZE);
                    arenaSlots = arena ? SCRIPT_ARENA_BYTES / 8 : 0;
                    arenaUsed = 1;
                }

                INT64 slotAmmount = Val(L.b);
                if (slotAmmount < 1) slotAmmount = 1;

                if (!arena || arenaUsed + (UINT64)slotAmmount > arenaSlots) {
                    vars[L.a.value] = -1;
                } else {
                    vars[L.a.value] = (INT64)arenaUsed;
                    arenaUsed += slotAmmount;
                }
                pc++;
                break;
            }
            case OP_POKE: {
                INT64 slot = Val(L.a) + Val(L.b);
                if (arena && slot >= 1 && (UINT64)slot < arenaUsed) arena[slot] = Val(L.c);
                pc++;
                break;
            }
            case OP_PEEK: {
                INT64 slot = Val(L.a) + Val(L.b);
                vars[L.c.value] = (arena && slot >= 1 && (UINT64)slot < arenaUsed) ? arena[slot] : 0;
                pc++;
                break;
            }
            case OP_FLIP: pc++; return true;
            case OP_END: return false;
            default: pc++; break;
        }
    }
    return false;
}

void Script::FreeArena() {
    if (!arena) return;

    Frames::Free(arena, SCRIPT_ARENA_BYTES / FRAME_SIZE);
    arena = 0;
    arenaSlots = 0;
    arenaUsed = 0;
}

void Script::RunLoop(bool allowEsc) {
    Keyboard::ClearStates();
    Graphics::back.clear(0x00000000);

    while (1) {
        Keyboard::PumpState();
        if (allowEsc && Keyboard::IsDown(KEY_ESC)) break;

        bool more = Step();

        Graphics::PresentFrame();
        Graphics::back.clear(0x00000000);

        if (!more) break;
    }

    FreeArena();
}
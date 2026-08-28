#include "parse.hh"
#include "../Script/Script.hh"

enum tokKind : UINT8 {
    TK_END = 0,
    TK_WORD,
    TK_STRING,
    TK_PUNCT,
};

struct LexTok {
    tokKind kind;
    char punct;
    char punct2;
    Token word;
    const char *at;
    UINT32 line;
};

static const char *sp;
static const char *send;
static UINT32 srcLine;
static LexTok look[3];
static UINT32 lookCount;
static UINT32 badLine;
static UINT32 tempNext;
static bool Expr(ScriptArg *out);
static bool Statement();

static bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool isPunct(char c) {
    return c == '{' || c == '}' || c == '(' || c == ')' || c == ';' || c == ','
        || c == '=' || c == '+' || c == '-' || c == '*' || c == '/' || c == '<'
        || c == '>' || c == '!' || c == '[' || c == ']'; 
}

static bool IsTemp(const ScriptArg &a) {
    return a.isVar && Script::varNames[a.value][0] == '$';
}

static bool NewTemp(ScriptArg *out) {
    if (tempNext >= 10) return false;

    char c = (char)('0' + tempNext);
    tempNext++;

    for (UINT32 i = 0; i < Script::varCount; i++) {
        if (Script::varNames[i][0] == '$' && Script::varNames[i][1] == c && Script::varNames[i][2] == '\0') {
            out->isVar = true;
            out->value = (INT64)i;
            return true;
        }
    }

    if (Script::varCount >= SCRIPT_MAX_VARS) return false;

    UINT32 slot = Script::varCount++;
    Script::varNames[slot][0] = '$';
    Script::varNames[slot][1] = c;
    Script::varNames[slot][2] = '\0';
    Script::vars[slot] = 0;

    out->isVar = true;
    out->value = (INT64)slot;
    return true;
}

static LexTok LexOne() {
    LexTok t;
    t.kind = TK_END;
    t.punct = 0;
    t.punct2 = 0;
    t.word.start = 0;
    t.word.lenght = 0;

    for (;;) {
        while (sp < send && IsSpace(*sp)) {
            if (*sp == '\n') srcLine++;
            sp++;
        }

        if (sp + 1 < send && sp[0] == '/' && sp[1] == '/') {
            while (sp < send && *sp != '\n') sp++;
            continue;
        }

        if (sp + 1 < send && sp[0] == '/' && sp[1] == '*') {
            sp += 2;
            while (sp + 1 < send && !(sp[0] == '*' && sp[1] == '/')) {
                if (*sp == '\n') srcLine++;
                sp++;
            }
            sp = (sp + 1 < send) ? sp + 2 : send;
            continue;
        }
        break;
    }

    t.line = srcLine;
    t.at = sp;
    if (sp >= send) return t;

    if (*sp == '"') {
        sp++;
        t.kind = TK_STRING;
        t.word.start = sp;
        while (sp < send && *sp != '"') sp++;
        t.word.lenght = (UINT32)(sp - t.word.start);
        if (sp < send) sp++;
        return t;
    }

    if (isPunct(*sp)) {
        t.kind = TK_PUNCT;
        t.punct = *sp;
        sp++;

        if (sp < send && *sp == '=' && (t.punct == '=' || t.punct == '!' || t.punct == '<' || t.punct == '>')) {
            t.punct2 = '=';
            sp++;
        }
        return t;
    }

    t.kind = TK_WORD;
    t.word.start = sp;
    while (sp < send && !IsSpace(*sp) && !isPunct(*sp) && *sp != '"') sp++;
    t.word.lenght = (UINT32)(sp - t.word.start);
    return t;
}

static LexTok &Peek(UINT32 i) {
    while (lookCount <= i) {
        look[lookCount] = LexOne();
        lookCount++;
    }
    return look[i];
}

static LexTok Next() {
    LexTok t = Peek(0);
    for (UINT32 k = 1; k < lookCount; k++) look[k - 1] = look[k];
    if (lookCount > 0) lookCount--;
    return t;
}

static const char *badMsg;
static const char *badAt;

static bool Fail(const LexTok &t, const char *what) {
    if (!badLine) {
        badLine = t.line;
        badMsg = what;
        badAt = t.at;
    }
    return false;
}

static bool IsWord(const LexTok &t, const char *w) {
    if (t.kind != TK_WORD) return false;
    UINT32 i = 0;
    while (i < t.word.lenght && w[i]) {
        if (t.word.start[i] != w[i]) return false;
        i++;
    }
    return i == t.word.lenght && w[i] == '\0';
}

static bool EmitLabel(const Token &name) {
    if (name.lenght == 0 || name.lenght >= SCRIPT_NAME_LENGHT) return false;
    if (Script::lableCount >= SCRIPT_MAX_LABLES) return false;

    Script::Lable &lb = Script::lables[Script::lableCount++];
    for (UINT32 i = 0; i < name.lenght; i++) lb.name[i] = name.start[i];
    lb.name[name.lenght] = '\0';
    lb.line = Script::lineCount;
    return true;
}

static bool EmitRef(UINT8 op, const Token &name, UINT32 line) {
    if (Script::lineCount >= SCRIPT_MAX_LINES) return false;

    ScriptLine &L = Script::lines[Script::lineCount++];
    L.op = op;
    L.target = -1;
    L.srcLine = line;
    L.ref = name.start;
    L.refLenght = name.lenght;
    return true;
}

static bool EmitBare(UINT8 op, UINT32 line) {
    if (Script::lineCount >= SCRIPT_MAX_LINES) return false;

    ScriptLine &L = Script::lines[Script::lineCount++];
    L.op = op;
    L.target = -1;
    L.srcLine = line;
    L.ref = 0;
    L.refLenght = 0;
    return true;
}

static bool EmitOp2(UINT8 op, const ScriptArg &a, const ScriptArg &b, UINT32 line) {
    if (Script::lineCount >= SCRIPT_MAX_LINES) return false;

    ScriptLine &L = Script::lines[Script::lineCount++];
    L.op = op;
    L.a = a;
    L.b = b;
    L.target = -1;
    L.srcLine = line;
    L.ref = 0;
    L.refLenght = 0;
    return true;
}

static bool EmitOp3(UINT8 op, const ScriptArg &a, const ScriptArg &b, const ScriptArg &c, UINT32 line) {
    if (Script::lineCount >= SCRIPT_MAX_LINES) return false;

    ScriptLine &L = Script::lines[Script::lineCount++];
    L.op = op;
    L.a = a;
    L.b = b;
    L.c = c;
    L.target = -1;
    L.srcLine = line;
    L.ref = 0;
    L.refLenght = 0;
    return true;
}

static bool Materialize(ScriptArg *v, const LexTok &t) {
    if (IsTemp(*v)) return true;

    ScriptArg s;
    if (!NewTemp(&s)) return Fail(t, "expression too complex");
    if (!EmitOp2(OP_SET, s, *v, t.line)) return Fail(t, "script too long");
    *v = s;
    return true;
}

static bool Primary(ScriptArg *out) {
    LexTok t = Peek(0);

    if (t.kind == TK_PUNCT && t.punct == '(') {
        Next();
        if (!Expr(out)) return false;
        if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ')') return Fail(Peek(0), "expected ')'");
        Next();
        return true;
    }

    if (t.kind != TK_WORD) return Fail(t, "expected a value");
    Next();
    if (!Script::ParseArg(t.word, out)) return Fail(t, "not a valid number or name");

    if (Peek(0).kind == TK_PUNCT && Peek(0).punct == '[') {
        Next();

        ScriptArg idx;
        if (!Expr(&idx)) return false;

        if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ']') return Fail(Peek(0), "expected ']");
        Next();

        ScriptArg base = *out;
        if (!NewTemp(out)) return Fail(t, "expression too complex");
        if (!EmitOp3(OP_PEEK, base, idx, *out, t.line)) return Fail(t, "script too long");
    }
    return true;
}

static bool Unary(ScriptArg *out) {
    LexTok t = Peek(0);

    if (t.kind == TK_PUNCT && t.punct == '-') {
        Next();

        ScriptArg v;
        if (!Unary(&v)) return false;

        if (!v.isVar) {
            out->isVar = false;
            out->value = -v.value;
            return true;
        }

        ScriptArg zero;
        zero.isVar = false;
        zero.value = 0;

        if (!NewTemp(out)) return Fail(t, "expression too complex");
        if (!EmitOp2(OP_SET, *out, zero, t.line)) return Fail(t, "script too long");
        if (!EmitOp2(OP_SUB, *out, v, t.line)) return Fail(t, "script too long");
        return true;
    }

    return Primary(out);
}

static bool Term(ScriptArg *out) {
    if (!Unary(out)) return false;

    for (;;) {
        LexTok t = Peek(0);
        if (t.kind != TK_PUNCT) return true;
        if (t.punct != '*' && t.punct != '/') return true;
        Next();

        if (!Materialize(out, t)) return false;

        ScriptArg rhs;
        if (!Unary(&rhs)) return false;
        if (!EmitOp2(t.punct == '*' ? OP_MUL : OP_DIV, *out, rhs, t.line)) return Fail(t, "script too long");
    }
}

static bool Expr(ScriptArg *out) {
    if (!Term(out)) return false;

    for (;;) {
        LexTok t = Peek(0);
        if (t.kind != TK_PUNCT) return true;
        if (t.punct != '+' && t.punct != '-') return true;
        Next();

        if (!Materialize(out, t)) return false;

        ScriptArg rhs;
        if (!Term(&rhs)) return false;
        if (!EmitOp2(t.punct == '+' ? OP_ADD : OP_SUB, *out, rhs, t.line)) return Fail(t, "script too long");
    }
}

static bool IsType(const LexTok &t) {
    return IsWord(t, "byte") || IsWord(t, "short") || IsWord(t, "int") || IsWord(t, "long");
}

static INT32 EmitIf(const ScriptArg &a, const ScriptArg &b, char cmp, UINT8 neg, UINT32 line) {
    if (Script::lineCount >= SCRIPT_MAX_LINES) return -1;

    INT32 at = (INT32)Script::lineCount;
    ScriptLine &L = Script::lines[Script::lineCount++];
    L.op = OP_IF;
    L.cmp = cmp;
    L.neg = neg;
    L.a = a;
    L.b = b;
    L.target = -1;
    L.srcLine = line;
    L.ref = 0;
    L.refLenght = 0;
    return at;
}

static bool Condition(ScriptArg *a, ScriptArg *b, char *cmp, UINT32 line) {
    if (Peek(0).kind != TK_PUNCT || Peek(0).punct != '(') return Fail(Peek(0), "expected '(' after if");
    Next();

    if (!Expr(a)) return false;

    LexTok op = Peek(0);
    char c = 0;
    if (op.kind == TK_PUNCT) {
        if (op.punct == '=' && op.punct2 == '=') c = '=';
        else if (op.punct == '!' && op.punct2 == '=') c = '!';
        else if (op.punct == '<' && op.punct2 == '=') c = 'l';
        else if (op.punct == '>' && op.punct2 == '=') c = 'g';
        else if (op.punct == '<' && op.punct2 == 0) c = '<';
        else if (op.punct == '>' && op.punct2 == 0) c = '>';
    }
    if (!c) return Fail(op, "expected a comparison operator");
    Next();

    if (!Expr(b)) return false;

    if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ')') return Fail(Peek(0), "expected ')'");
    Next();

    *cmp = c;
    return true;
}

static bool Block(UINT32 line) {
    if (Peek(0).kind != TK_PUNCT || Peek(0).punct != '{') return Fail(Peek(0), "expected '{'");
    LexTok brace = Next();

    while (!(Peek(0).kind == TK_PUNCT && Peek(0).punct == '}')) {
        if (Peek(0).kind == TK_END) return Fail(brace, "unclosed '}'");
        if (!Statement()) return false;
    }
    Next();
    return true;
}

static bool IfStatement() {
    LexTok kw = Next();


    ScriptArg a, b;
    char cmp;

    if (!Condition(&a, &b, &cmp, kw.line)) return false;
    if (IsWord(Peek(0), "goto")) {
        Next();

        LexTok target = Next();
        if (target.kind != TK_WORD) return Fail(target, "expected a label name after goto");
        if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");
        Next();

        INT32 at = EmitIf(a, b, cmp, 0, kw.line);
        if (at < 0) return Fail(kw, "script too long");

        Script::lines[at].ref = target.word.start;
        Script::lines[at].refLenght = target.word.lenght;
        return true;
    }

    INT32 skip = EmitIf(a, b, cmp, 1, kw.line);
    if (skip < 0) return Fail(kw, "script too long");

    if (!Block(kw.line)) return false;

    if (IsWord(Peek(0), "else")) {
        Next();

        UINT32 jmp = Script::lineCount;
        if (!EmitBare(OP_GOTO, kw.line)) return Fail(kw, "script too long");

        Script::lines[skip].target = (INT32)Script::lineCount;

        tempNext = 0;
        if (IsWord(Peek(0), "if")) {
            if (!IfStatement()) return false;
        } else {
            if (!Block(kw.line)) return false;
        }

        Script::lines[jmp].target = (INT32)Script::lineCount;
        return true;
    }

    Script::lines[skip].target = (INT32)Script::lineCount;
    return true;
}

static bool WhileStatement() {
    LexTok kw = Next();

    UINT32 top = Script::lineCount;

    ScriptArg a, b;
    char cmp;
    if (!Condition(&a, &b, &cmp, kw.line)) return false;

    INT32 exit = EmitIf(a, b, cmp, 1, kw.line);
    if (exit < 0) return Fail(kw, "script too long");

    if (!Block(kw.line)) return false;

    UINT32 back = Script::lineCount;
    if (!EmitBare(OP_GOTO, kw.line)) return Fail(kw, "script too long");
    Script::lines[back].target = (INT32)top;

    Script::lines[exit].target = (INT32)Script::lineCount;
    return true;
}

static bool Statement() {
    LexTok first = Peek(0);
    if (first.kind == TK_END) return Fail(first, "unexpected end of file");

    tempNext = 0;

    if (IsWord(first, "if")) return IfStatement();
    if (IsWord(first, "while")) return WhileStatement();

    if(first.kind == TK_WORD && Peek(1).kind == TK_PUNCT && Peek(1).punct == '[') {
        LexTok name = Next();
        Next();

        ScriptArg base;
        if (!Script::ParseArg(name.word, &base)) return Fail(name, "not a valid variable name");

        ScriptArg idx;
        if (!Expr(&idx)) return false;

        if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ']') return Fail(Peek(0), "expected ']");
        Next();

        if (Peek(0).kind != TK_PUNCT || Peek(0).punct != '=' || Peek(0).punct2 != 0) return Fail(Peek(0), "expected '=' after ']'");
        Next();

        ScriptArg val;
        if (!Expr(&val)) return false;

        if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");
        Next();

        if (!EmitOp3(OP_POKE, base, idx, val, name.line)) return Fail(name, "script too long");
        return true;
    }

    UINT32 at = IsType(first) ? 1 : 0;

    if (IsWord(first, "byte")) {
        if (Peek(at).kind == TK_WORD && Peek(at + 1).kind == TK_PUNCT && Peek(at + 1).punct == '=' && Peek(at + 1).punct2 == 0) {
            if (at) Next();
            LexTok name = Next();
            Next();

            ScriptArg dst;
            if (!Script::ParseVar(name.word, &dst)) return Fail(name, "not a valid variable name");
            Script::varWidth[dst.value] = 1;

            ScriptArg val;
            if (!Expr(&val)) return false;

            if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");
            Next();

            if (!EmitOp2(OP_SET_BYTE, dst, val, name.line)) return Fail(name, "script too long");
            return true;
        }
    }

    if (IsWord(first, "short")) {
        if (Peek(at).kind == TK_WORD && Peek(at + 1).kind == TK_PUNCT && Peek(at + 1).punct == '=' && Peek(at + 1).punct2 == 0) {
            if (at) Next();
            LexTok name = Next();
            Next();

            ScriptArg dst;
            if (!Script::ParseVar(name.word, &dst)) return Fail(name, "not a valid variable name");
            Script::varWidth[dst.value] = 2;

            ScriptArg val;
            if (!Expr(&val)) return false;

            if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");
            Next();

            if (!EmitOp2(OP_SET_SHORT, dst, val, name.line)) return Fail(name, "script too long");
            return true;
        }
    }

    if (IsWord(first, "int")) {
        if (Peek(at).kind == TK_WORD && Peek(at + 1).kind == TK_PUNCT && Peek(at + 1).punct == '=' && Peek(at + 1).punct2 == 0) {
            if (at) Next();
            LexTok name = Next();
            Next();

            ScriptArg dst;
            if (!Script::ParseVar(name.word, &dst)) return Fail(name, "not a valid variable name");
            Script::varWidth[dst.value] = 4;

            ScriptArg val;
            if (!Expr(&val)) return false;

            if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");
            Next();

            if (!EmitOp2(OP_SET_INT, dst, val, name.line)) return Fail(name, "script too long");
            return true;
        }
    }

    if (IsWord(first, "long")) {
        if (Peek(at).kind == TK_WORD && Peek(at + 1).kind == TK_PUNCT && Peek(at + 1).punct == '=' && Peek(at + 1).punct2 == 0) {
            if (at) Next();
            LexTok name = Next();
            Next();

            ScriptArg dst;
            if (!Script::ParseVar(name.word, &dst)) return Fail(name, "not a valid variable name");
            Script::varWidth[dst.value] = 8;

            ScriptArg val;
            if (!Expr(&val)) return false;

            if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");
            Next();

            if (!EmitOp2(OP_SET_LONG, dst, val, name.line)) return Fail(name, "script too long");
            return true;
        }
    }

    if (!at && first.kind == TK_WORD && Peek(1).kind == TK_PUNCT && Peek(1).punct == '=' && Peek(1).punct2 == 0) {
        LexTok name = Next();
        Next();

        ScriptArg dst;
        if (!Script::ParseVar(name.word, &dst)) return Fail(name, "not a valid variable name");

        ScriptArg val;
        if (!Expr(&val)) return false;

        if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");
        Next();

        if (!EmitOp2(OP_SET, dst, val, name.line)) return Fail(name, "script too long");

       UINT8 w = Script::varWidth[dst.value];
        if (w == 1 || w == 2 || w == 4) {
            ScriptArg width;
            width.isVar = false;
            width.value = w;
            if (!EmitOp2(OP_TRUNC, dst, width, name.line)) return Fail(name, "script too long");
        }
        return true;
    }

    if (first.kind == TK_WORD && Peek(1).kind == TK_PUNCT && Peek(1).punct == '('
        && Peek(2).kind == TK_PUNCT && Peek(2).punct == ')') {
            Next();
            Next();
            Next();

            if (Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");

            Next();

            if (!EmitRef(OP_GOSUB, first.word, first.line)) return Fail(first, "script too long");
            
            return true;
    }

    Token toks[8];
    UINT32 n = 0;
    while (Peek(0).kind == TK_WORD || Peek(0).kind == TK_STRING) {
        if (n >= 8) return Fail(Peek(0), "too many words in statement");
        toks[n++] = Next().word;
    }
    if (Peek(0).kind != TK_PUNCT || Peek(0).punct != ';') return Fail(Peek(0), "expected ';'");
    Next();

    if (n == 0) return Fail(first, "expected a statement");
    if (!Script::ParseStatement(toks, n, first.line)) return Fail(first, "unknown or malformed command");
    return true;
}

static bool Sub() {
    LexTok kw = Next();
    LexTok name = Next();
    if (name.kind != TK_WORD) return Fail(name, "expected a name after sub");

    if (Peek(0).punct != '(') return Fail(Peek(0), "expected '('");
    Next();
    if (Peek(0).punct != ')') return Fail(Peek(0), "expected ')'");
    Next();

    UINT32 skip = Script::lineCount;
    if (!EmitBare(OP_GOTO, name.line)) return Fail(name, "script too long");

    if (!EmitLabel(name.word)) return Fail(name, "sub name too long or too many subs");

    if (!Block(name.line)) return false;

    if (!EmitBare(OP_RETURN, name.line)) return Fail(name, "script too long");

    Script::lines[skip].target = (INT32)Script::lineCount;
    return true;
}

static void PublishError(const char *source) {
    Parser::errorLine = badLine;
    Parser::errorMsg = badMsg ? badMsg : "syntax error";
    Parser::errorCol = 0;

    if (badAt && badAt >= source) {
        const char *lineStart = badAt;
        while (lineStart > source && lineStart[-1] != '\n') lineStart--;
        Parser::errorCol = (UINT32)(badAt - lineStart);
    }
}

INT32 Parser::Load(const char *source, UINT64 length) {
    Script::ResetState();

    sp = source;
    send = source + length;
    srcLine = 1;
    lookCount = 0;
    badLine = 0;
    badMsg = 0;
    badAt = 0;

    errorMsg = 0;
    errorLine = 0;
    errorCol = 0;

    while (Peek(0).kind != TK_END) {
        if (IsWord(Peek(0), "sub")
            && Peek(1).kind == TK_WORD
            && Peek(2).kind == TK_PUNCT && Peek(2).punct == '(') {
            if (!Sub()) { PublishError(source); return (INT32)badLine; }
        } else {
            if (!Statement()) { PublishError(source); return (INT32)badLine; }
        }
    }

    INT32 unresolved = Script::ResolveLables();
    if (unresolved >= 0) {
        badLine = (UINT32)unresolved;
        badMsg = "unknown label or sub";
        badAt = 0;
        PublishError(source);
    }

    return unresolved;
}
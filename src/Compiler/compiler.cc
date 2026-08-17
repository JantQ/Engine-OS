#include "Compiler.hh"
#include "Emit.hh"

static Emit e;

static UINT32 VarDisp(INT64 slot) {
    return CTX_VARS + (UINT32)slot * 8;
}

static bool FitsImm32(INT64 val) {
    return val >= -2147483648LL && val <= 2147483647LL;
}

static void LoadReg(ScriptArg &args, UINT8 reg = REG_RAX ) {
    if (args.isVar) {
        e.MemRbx(0x8B, reg, VarDisp(args.value)); // mov rax, [rbx+disp]
    } else {
        e.U8(0x48);
        e.U8((UINT8)(0xb8 + reg)); // mov reg, imm64
        e.U64((UINT64)args.value);
    }
}

static void AddFixup(UINT64 site, INT32 target) {
    if (fixupCount >= SCRIPT_MAX_LINES) {
        e.overflow = true;
        return;
    }
    fixups[fixupCount].site = site;
    fixups[fixupCount].target = target;
    fixupCount++;
}

static void StoreArg(UINT32 index, ScriptArg &a) {
    if (!a.isVar && FitsImm32(a.value)) {
        e.MemRbx(0xC7, 0, CTX_ARGS + index * 8);
        e.U32((UINT32)(INT32)a.value);
    } else {
        LoadReg(a, REG_RAX);
        e.MemRbx(0x89, REG_RAX, CTX_ARGS + index * 8);
    }
}

static void CallFn(UINT32 index) {
    e.RegReg(0x89, REG_RBX, REG_RCX);
    e.MemRbx(0x8B, REG_RAX, CTX_FN + index * 8);
    e.CallReg(REG_RAX);
} 

UINT64 Compiler::Compile(UINT8 *out, UINT64 cap, UINT32 *badLine) {
    e.Init(out, cap);
    fixupCount = 0;
    *badLine = 0;

    e.U8(0x53); // push rbx
    e.U8(0x55);
    e.RegReg(0x89, REG_RSP, REG_RBP);
    e.RegImm8(0x83, 5, REG_RSP, 40); // sub rsp, 40 
    e.RegReg(0x89, REG_RCX, REG_RBX); // mov rbx, rcx

    for (UINT32 i = 0; i < Script::lineCount; i++) {
        lineOffset[i] = (UINT32)e.used;
        ScriptLine &Line = Script::lines[i];

        switch (Line.op) {
            case OP_SET:
                LoadReg(Line.b);
                e.MemRbx(0x89, REG_RAX, VarDisp(Line.a.value)); // mov [rbx+x], rax
                break;
            
            case OP_ADD:
                LoadReg(Line.b);
                e.MemRbx(0x01, REG_RAX, VarDisp(Line.a.value)); // add [rbx+x], rax
                break;

            case OP_SUB: 
                LoadReg(Line.b);
                e.MemRbx(0x29, REG_RAX, VarDisp(Line.a.value)); // sub [rbx+x], rax
                break;

            case OP_MUL:
                LoadReg(Line.b);
                e.MemRbx0F(0xAF, REG_RAX, VarDisp(Line.a.value)); // imul rax, [rbx+x]
                e.MemRbx(0x89, REG_RAX, VarDisp(Line.a.value)); // mov [rbx+x], rax
                break;

            case OP_DIV: {

                UINT32 dst = VarDisp(Line.a.value);
                if (!Line.b.isVar) {
                    if (Line.b.value == 0) {
                        e.MemRbx(0xC7, 0, dst); // mov qword [rbx+x], 0
                        e.U32(0);
                        break;
                    }
                    LoadReg(Line.b, REG_RCX);
                    e.MemRbx(0x8B, REG_RAX, dst); // mov rax, [rbx+x]
                    e.U8(0x48); // cqo
                    e.U8(0x99);
                    e.RegReg(0xF7, 7, REG_RCX); // idiv rcx
                    e.MemRbx(0x89, REG_RAX, dst); // mov [rbx+x], rax
                    break;
                }

                LoadReg(Line.b, REG_RCX);
                e.RegReg(0x85, REG_RCX, REG_RCX); // test rcx, rcx
                UINT64 zero = e.Rel8(0x74); // jz -> zero path

                e.MemRbx(0x8B, REG_RAX, dst); // mov rax, [rbx+x]
                e.U8(0x48); // cqo
                e.U8(0x99);
                e.RegReg(0xF7, 7, REG_RCX); // idiv rcx
                e.MemRbx(0x89, REG_RAX, dst); // mov [rbx+x], rax
                UINT64 done = e.Rel8(0xEB); // jmp -> done

                e.Patch8(zero);
                e.MemRbx(0xC7, 0, dst); //mov qword [rbx+x], 0
                e.U32(0);
                e.Patch8(done);
                break;
            }
            case OP_RND: {
                UINT32 dst = VarDisp(Line.a.value);

                e.MemRbx(0x8B, REG_RAX, CTX_RNG); // mov rax, [rbx+rng]

                e.RegReg(0x89, REG_RAX, REG_RCX); // mov rcx, rax
                e.RegImm8(0xC1, 4, REG_RCX, 13);  // shl rcx, 13
                e.RegReg(0x31, REG_RCX, REG_RAX); // xor rax, rcx

                e.RegReg(0x89, REG_RAX, REG_RCX); // mov rcx, rax
                e.RegImm8(0xC1, 5, REG_RCX, 7); // s0hr rcx, 7
                e.RegReg(0x31, REG_RCX, REG_RAX); // xor rax, rcx

                e.RegReg(0x89, REG_RAX, REG_RCX); // mov rcx, rax
                e.RegImm8(0xC1, 4, REG_RCX, 17); // shl rcx, 17
                e.RegReg(0x31, REG_RCX, REG_RAX); // xor rax, rcx

                e.MemRbx(0x89, REG_RAX, CTX_RNG); // mov [rbx+rng], rax

                LoadReg(Line.b, REG_RCX); // rcx = max
                e.RegReg(0x85, REG_RCX, REG_RCX); // test rcx, rcx
                UINT64 zero = e.Rel8(0x7E); // jle -> zero path

                e.RegReg(0x31, REG_RDX, REG_RDX); // xor rdx, rdx
                e.RegReg(0xF7, 6, REG_RCX); // div rcx   (unsigned!)
                e.MemRbx(0x89, REG_RDX, dst); // mov [rbx+x], rdx
                UINT64 done = e.Rel8(0xEB); // jmp -> done

                e.Patch8(zero);
                e.MemRbx(0xC7, 0, dst);
                e.U32(0);
                e.Patch8(done);
                break;
            }               

            case OP_GOTO:
                AddFixup(e.Rel32(0xE9), Line.target); // jmp rel32
                break;
            
            case OP_IF: {
                LoadReg(Line.a, REG_RAX);

                if (Line.b.isVar) {
                    e.MemRbx(0x3B, REG_RAX, VarDisp(Line.b.value)); // cmp rax, [rbx+y]
                } else if (FitsImm32(Line.b.value)) {
                    e.RegReg(0x81, 7, REG_RAX); // cmp rax, imm32
                    e.U32((UINT32)(INT32)Line.b.value);
                } else {
                    LoadReg(Line.b, REG_RCX);
                    e.RegReg(0x39, REG_RCX, REG_RAX); // cmp rax, rcx
                }

                UINT8 cc;
                if (Line.cmp == '=') cc = 0x84; // je
                else if (Line.cmp == '!') cc = 0x85; // jne
                else if (Line.cmp == '<') cc = 0x8C; // jl
                else cc= 0x8F; // jg

                AddFixup(e.Rel32_0F(cc), Line.target);
                break;
            }

            case OP_KEY:
                LoadReg(Line.b, REG_RCX);
                e.MemRbx(0x8B, REG_RAX, CTX_FN + FN_KEYDOWN * 8);
                e.CallReg(REG_RAX);
                e.MemRbx(0x89, REG_RAX, VarDisp(Line.a.value));
                break;
            
            case OP_SCREEN: 
                e.MemRbx(0x8B, REG_RAX, CTX_FN + FN_SCREENW * 8);
                e.CallReg(REG_RAX);
                e.MemRbx(0x89, REG_RAX, VarDisp(Line.a.value));

                e.MemRbx(0x8B, REG_RAX, CTX_FN + FN_SCREENH * 8);
                e.CallReg(REG_RAX);
                e.MemRbx(0x89, REG_RAX, VarDisp(Line.b.value));
                break;

            case OP_MILLIS: 
                e.MemRbx(0x8B, REG_RAX, CTX_FN + FN_MILLIS * 8);
                e.CallReg(REG_RAX);
                e.MemRbx(0x89, REG_RAX, VarDisp(Line.a.value));
                break;

            case OP_WAIT:
                LoadReg(Line.a, REG_RCX);
                e.MemRbx(0x8B, REG_RAX, CTX_FN + FN_WAIT * 8);
                e.CallReg(REG_RAX);
                break;
            
            case OP_END:
                AddFixup(e.Rel32(0xE9), (INT32)Script::lineCount); // jmp epilogue
                break;

            case OP_RECT:
                StoreArg(0, Line.a);
                StoreArg(1, Line.b);
                StoreArg(2, Line.c);
                StoreArg(3, Line.d);
                StoreArg(4, Line.e);
                CallFn(FN_RECT);
                break;

            case OP_TEXT:
                StoreArg(0, Line.a);
                StoreArg(1, Line.b);
                e.MemRbx(0xC7, 0, CTX_ARGS + 2 * 8);
                e.U32(Line.strOffset);
                CallFn(FN_TEXT);
                break;

            case OP_NUM:
                StoreArg(0, Line.a);
                StoreArg(1, Line.b);
                StoreArg(2, Line.c);
                CallFn(FN_NUM);
                break;
            
            case OP_FLIP:
                CallFn(FN_FLIP);
                e.RegReg(0x85, REG_RAX, REG_RAX);
                AddFixup(e.Rel32_0F(0x84), (INT32)Script::lineCount);
                break;

            case OP_ALLOC:
                StoreArg(0, Line.b);
                CallFn(FN_ALLOC);
                e.MemRbx(0x89, REG_RAX, VarDisp(Line.a.value));
                break;  

            case OP_POKE:
                StoreArg(0, Line.a);
                StoreArg(1, Line.b);
                StoreArg(2, Line.c);
                CallFn(FN_POKE);
                break;

            case OP_PEEK:
                StoreArg(0, Line.a);
                StoreArg(1, Line.b);
                CallFn(FN_PEEK);
                e.MemRbx(0x89, REG_RAX, VarDisp(Line.c.value));
                break;

            case OP_GOSUB:
                e.RegImm8(0x83, 5, REG_RSP, 8);
                AddFixup(e.Rel32(0xE8), Line.target);
                e.RegImm8(0x83, 0, REG_RSP, 8);
                break;

            case OP_RETURN:
                e.U8(0xC3);
                break;
            
            default:
                *badLine = Line.srcLine;
                return 0;
        }
    }
    lineOffset[Script::lineCount] = (UINT32)e.used;

    e.RegReg(0x89, REG_RBP, REG_RSP);
    e.U8(0x5D);
    e.U8(0x5B);
    e.U8(0xC3);

    if (e.overflow) return 0;
    
    for (UINT32 f = 0; f < fixupCount; f++) {
        e.Patch32(fixups[f].site, lineOffset[fixups[f].target]);
    }

    if (e.overflow) return 0;
    return e.used;
}
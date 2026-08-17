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

UINT64 Compiler::Compile(UINT8 *out, UINT64 cap, UINT32 *badLine) {
    e.Init(out, cap);
    *badLine = 0;

    e.U8(0x53); // push rbx
    e.RegImm8(0x83, 5, REG_RSP, 32); // sub rsp, 32 
    e.RegReg(0x89, REG_RCX, REG_RBX); // mov rbx, rcx

    for (UINT32 i = 0; i < Script::lineCount; i++) {
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

            
            case OP_END:
                break;

            default:
                *badLine = Line.srcLine;
                return 0;
        }
    }
    e.RegImm8(0x83, 0, REG_RSP, 32); // add rsp, 32
    e.U8(0x5B); // pop rbx
    e.U8(0xC3); // ret

    if (e.overflow) return 0;
    return e.used;
}
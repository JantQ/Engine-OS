#pragma once

#include "../kernel.hh"

#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6
#define REG_RDI 7

class Emit {
    public:
        UINT8 *buffer;
        UINT64 cap;
        UINT64 used;
        bool overflow;

        void Init(UINT8 *b, UINT64 c) {
            buffer = b;
            cap = c;
            used = 0;
            overflow = false;
        }

        void U8(UINT8 val) {
            if (used >= cap) {
                overflow = true;
                return;
            }
            buffer[used++] = val;
        }

        void U32(UINT32 val) {
            U8((UINT8)val);
            U8((UINT8)(val >> 8));
            U8((UINT8)(val >> 16));
            U8((UINT8)(val >> 24));
        }

        void U64(UINT64 val) {
            U32((UINT32)val);
            U32((UINT32)(val >> 32));
        }

        void MemRbx(UINT8 op, UINT8 regField, UINT32 disp) {
            U8(0x48);
            U8(op);
            U8((UINT8)(0x80 | (regField << 3) | REG_RBX));
            U32(disp);
        }

        void MemRbx0F(UINT8 op, UINT8 regField, UINT32 disp) {
            U8(0x48);
            U8(0x0F);
            U8(op);
            U8((UINT8)(0x80 | (regField << 3) | REG_RBX));
            U32(disp);
        }

        void RegReg(UINT8 op, UINT8 reg, UINT8 rm) {
            U8(0x48);
            U8(op);
            U8((UINT8)(0xC0 | (reg << 3) | rm));
        }

        void RegImm8(UINT8 op, UINT8 digit, UINT8 rm, UINT8 imm) {
            RegReg(op, digit, rm);
            U8(imm);
        }

        UINT64 Rel8(UINT8 op) {
            U8(op);
            U8(0x00);
            return used - 1;
        }

        void Patch8(UINT64 site) {
            INT64 delta = (INT64)(used - (site + 1));
            if (delta < -128 || delta > 127) {
                overflow = true;
                return;
            }
            buffer[site] = (UINT8)(INT8)delta;
        }
};
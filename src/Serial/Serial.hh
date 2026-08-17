#pragma once

#include "../kernel.hh"
#include "../IO/io.hh"

#define COM1 0x3F8

class Serial {
    public:

        static void Print(char c) {
            if (c == '\n') {
                while (!(IO::inb(COM1 + 5) & 0x20)) {}
                IO::outb(COM1, '\r');
            }
            while (!(IO::inb(COM1 + 5) & 0x20)) {}
            IO::outb(COM1, (UINT8)c);
        }

        static void Print(const char *s) {
            while (*s) Print(*s++);
        }

        static void PrintHex(UINT64 value, UINT32 digits) {
            const char *hex = "0123456789ABCDEF";
            for (UINT32 i = 0; i < digits; i++) {
                Print(hex[(value >> ((digits - 1 - i) * 4)) & 0xF]);
            }
        }

        static void PrintUInt(UINT64 value) {
            if (value == 0) {
                Print('0');
                return;
            }
            char buf[21];
            UINT32 i = 0;
            while (value > 0) {
                buf[i++] = '0' + (char)(value % 10);
                value /= 10;
            }
            while (i > 0) Print(buf[--i]);
        }
};
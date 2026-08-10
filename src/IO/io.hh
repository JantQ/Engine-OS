#pragma once

#include "../kernel.hh"

class IO {
    public:
        static inline UINT8 inb(UINT16 port) {
            UINT8 result;
            __asm__ __volatile__("inb %1, %0" : "=a"(result) : "Nd"(port));
            return result;
        }

        static inline void outb(UINT16 port, UINT8 value) {
            __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
        }


        static inline UINT32 inl(UINT16 port) {
            UINT32 result;
            __asm__ __volatile__("inl %1, %0" : "=a"(result) : "Nd"(port));
            return result;
        }

        static inline void outl(UINT16 port, UINT32 value) {
            __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
        }

        static inline UINT64 ReadMsr(UINT32 msr) {
            UINT32 lo, hi;
            __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
            return ((UINT64)hi << 32) | lo;
        }

        static inline void WriteMsr(UINT32 msr, UINT64 value) {
            __asm__ __volatile__("wrmsr" :: "c"(msr), "a"((UINT32)value), "d"((UINT32)(value >> 32)));
        }
};
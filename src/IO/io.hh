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
};
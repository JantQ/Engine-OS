#pragma once

#include "../kernel.hh"

class Clock{
    public: 
        static void StartClock(EFI_SYSTEM_TABLE *SystemTable);
        static inline UINT64 tps;
        static inline UINT64 rdtsc() {
            UINT32 lo, hi;
            __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
            return ((UINT64)hi << 32) | lo;
        }
};
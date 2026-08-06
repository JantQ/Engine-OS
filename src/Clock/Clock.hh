#pragma once

#include "../kernel.hh"
#include "../IO/io.hh"

class Clock{
    public: 
        static inline UINT64 tps;
        static inline UINT64 bootTsc;

        static inline UINT64 Ticks() { return rdtsc() - bootTsc;}

        static inline UINT64 Millis() {
            UINT64 d = Ticks();
            UINT64 secs = d / tps;
            UINT64 rem = d % tps;
            return secs * 1000ULL + (rem * 1000ULL) / tps;
        }

        static inline UINT64 Micros() {
            UINT64 d = Ticks();
            UINT64 secs = d / tps;
            UINT64 rem = d % tps;
            return secs * 1000000ULL + (rem * 1000000ULL) / tps;
        }

        static inline void Delay(UINT64 ms) {
            UINT64 target = Millis() + ms;
            while (Millis() < target) __asm__ __volatile__ ("pause");
        }

        static inline UINT64 rdtsc() {
            UINT32 lo, hi;
            __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
            return ((UINT64)hi << 32) | lo;
        }

        static inline UINT16 readPitCount() {
            IO::outb(0x43, 0x00);
            UINT8 lo = IO::inb(0x40);
            UINT8 hi = IO::inb(0x40);
            return ((UINT16)hi << 8) | lo;
        }

        static void StartClock();

        static inline double GetDeltaTime(UINT64 lastTick, UINT64 currentTick) {
            return (double)(currentTick - lastTick) / (double)tps;
        }
};
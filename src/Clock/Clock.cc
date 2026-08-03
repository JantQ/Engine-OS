#include "Clock.hh"


void Clock::StartClock() {
    const UINT32 PIT_FREQ = 1193182;
    const UINT32 divisor = 11932;

    outb(0x43, 0x34);
    outb(0x40, (UINT8)(divisor & 0xFF));
    outb(0x40, (UINT8)((divisor >> 8) & 0xFF));

    UINT64 tsc_Start = rdtsc();

    UINT16 lsat = readPitCount();
    UINT16 current;

    do {
        current = readPitCount();
    } while (current <= lsat);

    UINT64 tsc_End = rdtsc();

    double interval_seconds = (double)divisor / (double)PIT_FREQ;
    tps = (UINT64)((double)(tsc_End - tsc_Start) / interval_seconds);
    bootTsc = rdtsc();
}
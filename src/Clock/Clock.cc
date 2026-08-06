#include "Clock.hh"

void Clock::StartClock() {
    const UINT32 PIT_FREQ = 1193182;
    const UINT16 ticks = 11932;              

    UINT8 p61 = IO::inb(0x61);
    IO::outb(0x61, (UINT8)((p61 & ~0x02) | 0x01));

    IO::outb(0x43, 0xB0);
    IO::outb(0x42, (UINT8)(ticks & 0xFF));
    IO::outb(0x42, (UINT8)(ticks >> 8));

    p61 = IO::inb(0x61);
    IO::outb(0x61, (UINT8)(p61 & ~0x01));
    IO::outb(0x61, (UINT8)(p61 | 0x01));

    UINT64 start = rdtsc();
    while (!(IO::inb(0x61) & 0x20)) { }         
    UINT64 end = rdtsc();

    double seconds = (double)ticks / (double)PIT_FREQ;
    tps = (UINT64)((double)(end - start) / seconds);
    bootTsc = rdtsc();
}
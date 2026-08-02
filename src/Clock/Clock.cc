#include "Clock.hh"

void Clock::StartClock(EFI_SYSTEM_TABLE *SystemTable) {
    UINT64 start = rdtsc();
    SystemTable->BootServices->Stall(1000000);
    tps = rdtsc() - start;
}
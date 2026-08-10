#pragma once

#include "../kernel.hh"

#define IA32_PAT 0x277
#define PAT_WC_SLOT1 0x0007040600070106ULL

class Paging {
    public:
        static void EnableWriteCombining();
        static UINT32 MarkRangeWc(UINT64 base, UINT64 size);
        static UINT64 EntryFor(UINT64 addr, UINT32 *level);

        static inline UINT64 ReadCr3() {
            UINT64 v;
            __asm__ __volatile__("mov %%cr3, %0" : "=r"(v));
            return v;
        }

        static inline void WriteCr3(UINT64 v) {
            __asm__ __volatile__("mov %0, %%cr3" : : "r"(v) : "memory");
        }

        static inline UINT64 ReadCr0() {
            UINT64 v;
            __asm__ __volatile__("mov %%cr0, %0" : "=r"(v));
            return v;
        }

        static inline void WriteCr0(UINT64 v) {
            __asm__ __volatile__("mov %0, %%cr0" :: "r"(v) : "memory");
        }

};
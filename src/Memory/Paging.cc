#include "Paging.hh"
#include "../IO/io.hh"

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_PWT (1ULL << 3)
#define PAGE_PCD (1ULL << 4)
#define PAGE_PS        (1ULL << 7)
#define PAGE_PAT_4K    (1ULL << 7)
#define PAGE_PAT_LARGE (1ULL << 12)
#define ADDR_MASK      0x000FFFFFFFFFF000ULL

static void SetWc(UINT64 *entry, bool large) {
    *entry |= PAGE_PWT;
    *entry &= ~PAGE_PCD;
    *entry &= ~(large ? PAGE_PAT_LARGE : PAGE_PAT_4K);
}

void Paging::EnableWriteCombining() {
    IO::WriteMsr(IA32_PAT, PAT_WC_SLOT1);
}

UINT32 Paging::MarkRangeWc(UINT64 base, UINT64 size) {
    UINT64 *pm14 = (UINT64*)(ReadCr3() & ADDR_MASK);
    UINT64 end = base + size;
    UINT32 changed = 0;

    UINT64 cr0 = ReadCr0();
    WriteCr0(cr0 & ~(1ULL << 16));

    for (UINT64 addr = base & ~0xFFFULL; addr < end; ) {
        UINT64 *pm14e = &pm14[(addr >> 39) & 0x1FF];
        if (!(*pm14e & PAGE_PRESENT)) {
            addr += 0x1000; 
            continue;
        }

        UINT64 *pdpt = (UINT64*)(*pm14e & ADDR_MASK);
        UINT64 *pdpte = &pdpt[(addr >> 30) & 0x1FF];
        if (!(*pdpte & PAGE_PRESENT)) {
            addr += 0x1000;
            continue;
        }

        if (*pdpte & PAGE_PS) {
            SetWc(pdpte, true);
            changed++;
            addr = (addr & ~0x3FFFFFFFULL) + 0x40000000ULL;
            continue;
        }

        UINT64 *pd = (UINT64*)(*pdpte & ADDR_MASK);
        UINT64 *pde = &pd[(addr >> 21) & 0x1FF];
        if (!(*pde & PAGE_PRESENT)) {
            addr += 0x1000;
            continue;
        }

        if (*pde & PAGE_PS) {
            SetWc(pde, true);
            changed++;
            addr = (addr & ~0x1FFFFFULL) + 0x200000ULL;
            continue;
        }

        UINT64 *pt = (UINT64*)(*pde & ADDR_MASK);
        UINT64 *pte = &pt[(addr >> 12) & 0x1FF];
        if (*pte & PAGE_PRESENT) {
            SetWc(pte, false);
            changed++;
        }
        addr += 0x1000;
    }

    WriteCr3(ReadCr3());
    WriteCr0(cr0);
    return changed;
}

UINT64 Paging::EntryFor(UINT64 addr, UINT32 *level) {
    UINT64 *pm14 = (UINT64*)(ReadCr3() & ADDR_MASK);

    UINT64 *pm14e = &pm14[(addr >> 39) & 0x1FF];
    if (!(*pm14e & PAGE_PRESENT)) {
        *level = 4;
        return 0;
    }

    UINT64 *pdpt = (UINT64*)(*pm14e & ADDR_MASK);
    UINT64 *pdpte = &pdpt[(addr >> 30) & 0x1FF];
    if (!(*pdpte & PAGE_PRESENT)) {
        *level = 3;
        return 0;
    }
    if (*pdpte & PAGE_PS) {
        *level = 3;
        return *pdpte;
    }

    UINT64 *pd = (UINT64*)(*pdpte & ADDR_MASK);
    UINT64 *pde = &pd[(addr >> 21) & 0x1FF];
    if (!(*pde & PAGE_PRESENT)) {
        *level = 2;
        return 0;
    }
    if (*pde & PAGE_PS) {
        *level = 2;
        return *pde;
    }

    UINT64 *pt = (UINT64*)(*pde & ADDR_MASK);
    *level = 1;
    return pt[(addr >> 12) & 0x1FF];
}
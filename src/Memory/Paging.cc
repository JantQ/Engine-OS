#include "Paging.hh"
#include "Framse.hh"
#include "../IO/io.hh"

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_RW (1ULL << 1)
#define PAGE_PWT (1ULL << 3)
#define PAGE_PCD (1ULL << 4)
#define PAGE_PS        (1ULL << 7)
#define PAGE_PAT_4K    (1ULL << 7)
#define PAGE_PAT_LARGE (1ULL << 12)
#define ADDR_MASK      0x000FFFFFFFFFF000ULL

#define TABEL_GIGS 512ULL
#define GiB 0x40000000ULL
#define BIG_PAGE 0x200000ULL

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

static UINT64 *NewTabel() {
    UINT64 *t = (UINT64*)Frames::Alloc(1);
    if (!t) return 0;
    for (UINT32 i = 0; i < 512; i++) t[i] = 0;
    return t;
}

static bool IsRamType(UINT32 type) {
    if (type == EfiLoaderCode) return true;
    if (type == EfiLoaderData) return true;
    if (type == EfiBootServicesCode) return true;
    if (type == EfiBootServicesData) return true;
    if (type == EfiRuntimeServicesCode) return true;
    if (type == EfiRuntimeServicesData) return true;
    if (type == EfiConventionalMemory) return true;
    return false;
}

bool Paging::BuildKernelTables(EFI_MEMORY_DESCRIPTOR *map, UINTN mapSize, UINTN descriptorSize) {
    UINT64 *pm14 = NewTabel();
    if (!pm14) return false;

    UINT64 *pdpt = NewTabel();
    if (!pdpt) return false;

    pm14[0] = (UINT64)pdpt | PAGE_PRESENT | PAGE_RW;

    for (UINT64 g = 0; g < TABEL_GIGS; g++) {
        UINT64 *pd = NewTabel();
        if (!pd) return false;

        pdpt[g] = (UINT64)pd | PAGE_PRESENT | PAGE_RW;

        for (UINT64 i = 0; i < 512; i++) {
            UINT64 addr = g * GiB + i * BIG_PAGE;
            pd[i] = addr | PAGE_PRESENT | PAGE_RW | PAGE_PS | PAGE_PCD;
        }
    }

    for (UINTN off = 0; off < mapSize; off += descriptorSize) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + off);
        if (!IsRamType(d->Type)) continue;

        UINT64 start = d->PhysicalStart & ~(BIG_PAGE - 1);
        UINT64 end = d->PhysicalStart + d->NumberOfPages * 4096ULL;

        for (UINT64 addr = start; addr < end && addr < TABEL_GIGS * GiB; addr += BIG_PAGE) {
            UINT64 *pd = (UINT64*)(pdpt[addr / GiB] & ADDR_MASK);
            pd[(addr >> 21) & 0x1FF] &= ~(PAGE_PCD | PAGE_PWT);
        }
    }

    kernelPm14 = (UINT64)pm14;
    return true;
}

bool Paging::MapUc(UINT64 base, UINT64 size) {
    if (!kernelPm14) return true;

    UINT64 *pm14 = (UINT64*)kernelPm14;
    UINT64 first = base & ~(BIG_PAGE - 1);
    UINT64 end = base + size;

    for (UINT64 addr = first; addr < end; addr += BIG_PAGE) {
        UINT64 *pm14e = &pm14[(addr >> 39) & 0x1FF];
        if (!(*pm14e & PAGE_PRESENT)) {
            UINT64 *t = NewTabel();
            if (!t) return false;
            *pm14e = (UINT64)t | PAGE_PRESENT | PAGE_RW;
        }

        UINT64 *pdpt = (UINT64*)(*pm14e & ADDR_MASK);
        UINT64 *pdpte = &pdpt[(addr >> 30) & 0x1FF];
        if (!(*pdpte & PAGE_PRESENT)) {
            UINT64 *t = NewTabel();
            if (!t) return false;
            *pdpte = (UINT64)t | PAGE_PRESENT | PAGE_RW;
        }

        UINT64 *pd = (UINT64*)(*pdpte & ADDR_MASK);
        pd[(addr >> 21) & 0x1FF] = addr | PAGE_PRESENT | PAGE_RW | PAGE_PS | PAGE_PCD;
    }

    if (ownTables) WriteCr3(ReadCr3());
    return true;
}

void Paging::Activate() {
    if (!kernelPm14) return;
    WriteCr3(kernelPm14);
    ownTables = true;
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
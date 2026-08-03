
#include "Heap.hh"

void Heap::Init(EFI_MEMORY_DESCRIPTOR *map, UINTN mapSize, UINTN descriptorSize) {
    UINT64 bestBytes = 0;
    UINT64 bestStart = 0;

    for (UINTN off = 0; off < mapSize; off += descriptorSize) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + off);
        if (d->Type != EfiConventionalMemory) continue;

        UINT64 bytes = d->NumberOfPages * 4096ULL;
        totalConv += bytes;
        convRegions++;

        if (d->PhysicalStart < 0x100000) continue;
        if (bytes > bestBytes) { bestBytes = bytes; bestStart = d->PhysicalStart; }
    }


    base = (UINT8*)bestStart;
    size = bestBytes;
    offset = 0;
}

void *Heap::Alloc(UINTN bytes, UINTN align) {
    UINTN aligned = (offset + (align - 1)) & ~(align - 1);
    if (aligned + bytes > size) return 0;
    offset = aligned + bytes;
    return base + aligned;
}

#include "Heap.hh"

void Heap::Init(EFI_MEMORY_DESCRIPTOR *map, UINTN mapSize, UINTN descriptorSize) {
    UINT64 bestBytes = 0;
    UINT64 bestStart = 0;

    totalConv = 0;
    convRegions = 0;

    for (UINTN off = 0; off < mapSize; off += descriptorSize) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + off);
        if (d->Type != EfiConventionalMemory) continue;

        UINT64 bytes = d->NumberOfPages * 4096ULL;
        totalConv += bytes;
        convRegions++;

        if (d->PhysicalStart < 0x100000) continue;
        if (bytes > bestBytes) { bestBytes = bytes; bestStart = d->PhysicalStart; }
    }

    UINTN take = bestBytes < HEAP_RESERVE_BYTES ? (UINTN)bestBytes : HEAP_RESERVE_BYTES;
    permanent.Init((void*)bestStart, take);
}
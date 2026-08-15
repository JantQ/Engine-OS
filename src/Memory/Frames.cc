#include "Framse.hh"

UINT64 Frames::BitmapBytesNeeded(EFI_MEMORY_DESCRIPTOR *map, UINTN mapSize, UINTN descriptorSize) {
    UINT64 highest = 0;

    for (UINTN off = 0; off < mapSize; off += descriptorSize) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + off);
        UINT64 end = d->PhysicalStart + d->NumberOfPages * FRAME_SIZE;
        if (end > highest) highest = end;
    }

    UINT64 frames = highest / FRAME_SIZE;
    return ((frames + 63) / 64) * 8;
}

void Frames::Init(EFI_MEMORY_DESCRIPTOR *map, UINTN mapSize, UINTN descriptorSize, void *bitmapMemory, UINT64 bitmapBytes) {
    bitmap = (UINT64*)bitmapMemory;
    if (!bitmap) return;

    UINT64 highest = 0;
    for (UINTN off = 0; off < mapSize; off += descriptorSize) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + off);
        UINT64 end = d->PhysicalStart + d->NumberOfPages * FRAME_SIZE;
        if (end > highest) highest = end;
    }
    frameCount = highest / FRAME_SIZE;

    UINT64 words = bitmapBytes / 8;
    for (UINT64 w = 0; w < words; w++) {
        bitmap[w] = ~0ULL;
    }

    freeFrames = 0;

    for (UINTN off = 0; off < mapSize; off += descriptorSize) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + off);
        if (d->Type != EfiConventionalMemory) continue;

        UINT64 first = d->PhysicalStart / FRAME_SIZE;
        for (UINT64 j = 0; j < d->NumberOfPages; j++) {
            UINT64 f = first + j;
            if (f >= frameCount) break;
            if (TestBit(f)) {
                ClearBit(f);
                freeFrames++;
            }
        }
    }

    Reserve(0, 0x100000);
    totalFrames = freeFrames;
}

void *Frames::Alloc(UINT64 count, UINT64 align) {
    if (!bitmap || count == 0) return 0;

    UINT64 step = align / FRAME_SIZE;
    if (step == 0) step = 1;

    for (UINT64 f = 0; f + count <= frameCount; f += step) {
        bool fits = true;
        for (UINT64 j = 0; j < count; j++) {
            if (TestBit(f + j)) { fits = false; break; }
        }
        if (!fits) continue;

        for (UINT64 j = 0; j < count; j++) SetBit(f + j);
        freeFrames -= count;
        return (void*)(f * FRAME_SIZE);
    }
    return 0;
}

void Frames::Free(void *addr, UINT64 count) {
    if (!bitmap) return;

    UINT64 first = (UINT64)addr / FRAME_SIZE;
    for (UINT64 j = 0; j < count; j++) {
        UINT64 f = first + j;
        if (f >= frameCount) break;
        if (TestBit(f)) {
            ClearBit(f);
            freeFrames++;
        }
    }
}

void Frames::Reserve(UINT64 addr, UINT64 bytes) {
    if (!bitmap) return;

    UINT64 first = addr / FRAME_SIZE;
    UINT64 frameAmmount = (bytes + FRAME_SIZE - 1) / FRAME_SIZE;

    for (UINT64 j = 0; j < frameAmmount; j++) {
        UINT64 f = first + j;
        if (f >= frameCount) break;
        if (!TestBit(f)) {
            SetBit(f);
            if (freeFrames > 0) freeFrames--;
        }
    }
}
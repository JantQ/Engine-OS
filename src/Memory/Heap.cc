
#include "Heap.hh"
#include "Framse.hh"

void Heap::Init() {
    UINT64 frameAmmount = HEAP_RESERVE_BYTES / FRAME_SIZE;

    void *base = Frames::Alloc(frameAmmount);
    if (!base) return;

    permanent.Init(base, HEAP_RESERVE_BYTES);
}

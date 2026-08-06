#include "Nvme.hh"
#include "../../Clock/Clock.hh"
#include "../../Memory/Heap.hh"

bool Nvme::Init(PciDevice *device) {
    if (!device) return false;

    Pci::EnableMasterBus(device->bus, device->device, device->func);

    bar0 = Pci::ReadBar(device->bus, device->device, device->func, 0);
    if (bar0 == 0) return false;


    UINT32 capLo = Read32(NVME_CAP);
    UINT32 capHi = Read32(NVME_CAP + 4);

    dstrd = capHi & 0xF;
    UINT64 timeoutMs = ((capLo >> 24) & 0xFF) * 500;

    Write32(NVME_INTMS, 0xFFFFFFFF);

    Write32(NVME_CC, Read32(NVME_CC) & ~1u);
    if (!WaitReady(false, timeoutMs)) return false;


    asq = (UINT8*)Heap::Alloc(QUEUE_ENTRIES * 64, 4096);
    acq = (UINT8*)Heap::Alloc(QUEUE_ENTRIES * 16, 4096);
    if (!asq || !acq) return false;

    ZeroBytes(asq, QUEUE_ENTRIES * 64);
    ZeroBytes(acq, QUEUE_ENTRIES * 16);

    Write32(NVME_AQA, ((QUEUE_ENTRIES - 1) << 16) | (QUEUE_ENTRIES - 1));
    Write64(NVME_ASQ, (UINT64)asq);
    Write64(NVME_ACQ, (UINT64)acq);

    UINT32 cc = (4 << 20) | (6 << 16) | (0 << 11) | (0 << 7) | (0 << 4) | 1;
    Write32(NVME_CC, cc);

    

    if (!WaitReady(true, timeoutMs)) return false;
    return true;
}

bool Nvme::WaitReady(bool wantReady, UINT64 timeoutMs) {
    UINT64 deadline = Clock::Millis() + timeoutMs;
    while (Clock::Millis() < deadline) {
        UINT32 csts = Nvme::Read32(NVME_CSTS);
        if (csts & 0x2) return false;
        if (((csts & 0x1) != 0) == wantReady) return true;
    }
    return false;
}
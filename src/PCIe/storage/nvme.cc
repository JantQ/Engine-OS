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


    admin.sq = (UINT8*)Heap::Alloc(QUEUE_ENTRIES * 64, 4096);
    admin.cq = (UINT8*)Heap::Alloc(QUEUE_ENTRIES * 16, 4096);
    if (!admin.sq || !admin.cq) return false;

    ZeroBytes(admin.sq, QUEUE_ENTRIES * 64);
    ZeroBytes(admin.cq, QUEUE_ENTRIES * 16);

    admin.qid = 0;
    admin.sqTail = 0;
    admin.cqHead = 0;
    admin.phase = 1;
    admin.entries = QUEUE_ENTRIES;

    Write32(NVME_AQA, ((QUEUE_ENTRIES - 1) << 16) | (QUEUE_ENTRIES - 1));
    Write64(NVME_ASQ, (UINT64)admin.sq);
    Write64(NVME_ACQ, (UINT64)admin.cq);

    UINT32 cc = (4 << 20) | (6 << 16) | (0 << 11) | (0 << 7) | (0 << 4) | 1;
    Write32(NVME_CC, cc);

    

    if (!WaitReady(true, timeoutMs)) return false;
    return true;
}

bool Nvme::Identify(UINT8 cns, UINT32 nsid, void *buffer) {
    UINT32 cmd[16];
    for (UINT32 i = 0; i < 16; i++) {
        cmd[i] = 0;
    }

    cmd[0] = NVME_ADMIN_IDENTIFY | ((UINT32)(nextCid++) << 16);
    cmd[1] = nsid;
    cmd[6] = (UINT32)((UINT64)buffer);
    cmd[7] = (UINT32)((UINT64)buffer >> 32);
    cmd[10] = cns;

    return Submit(admin, cmd) == 0;
}

UINT16 Nvme::Submit(NvmeQueue &queue, UINT32 *cmd) {
    volatile UINT32 *slot = (volatile UINT32*)(queue.sq + queue.sqTail * 64);

    for (UINT32 i = 0; i < 16; i++) {
        slot[i] = cmd[i];
    }

    __asm__ __volatile__("" : : : "memory");

    queue.sqTail = (queue.sqTail + 1) % queue.entries;
    Write32(SqTailDb(queue.qid), queue.sqTail);

    volatile UINT32 *cqe = (volatile UINT32*)(queue.cq + queue.cqHead * 16);
    UINT64 deadline = Clock::Millis() + 5000;
    UINT32 dw3;

    while(1) {
        dw3 = cqe[3];
        if (((dw3 >> 16) & 1) == queue.phase) break;
        if (Clock::Millis() > deadline) return 0xFFFF;
    }

    queue.cqHead = (queue.cqHead + 1) % queue.entries;
    if (queue.cqHead == 0) queue.phase ^= 1;
    Write32(CqHeadDb(queue.qid), queue.cqHead);

    return (UINT16)((dw3 >> 17)) & 0x7FF;
}

bool Nvme::CreateIoQueues() {
    io.sq = (UINT8*)Heap::Alloc(QUEUE_ENTRIES * 64, 4096);
    io.cq = (UINT8*)Heap::Alloc(QUEUE_ENTRIES * 64, 4096);

    if (!io.sq || !io.cq) return false;

    ZeroBytes(io.sq, QUEUE_ENTRIES * 64);
    ZeroBytes(io.cq, QUEUE_ENTRIES * 16);

    io.qid = 1;
    io.sqTail = 0;
    io.cqHead = 0;
    io.phase = 1;
    io.entries = QUEUE_ENTRIES;

    UINT32 cmd[16];

    // Comp queus
    for (UINT32 i = 0; i < 16; i++) {
        cmd[i] = 0;
    }
    cmd[0] = 0x05 | ((UINT32)(nextCid++) << 16);
    cmd[6] = (UINT32)(UINT64)io.cq;
    cmd[7] = (UINT32)((UINT64)io.cq >> 32);
    cmd[10] = io.qid | ((QUEUE_ENTRIES - 1) << 16);
    cmd[11] = 1;
    if (Submit(admin, cmd) != 0) return false;

    // Submissive quees
    for (UINT32 i = 0; i < 16; i++) {
        cmd[i] = 0;
    }
    cmd[0] = 0x01 | ((UINT32)(nextCid++) << 16);
    cmd[6] = (UINT32)(UINT64)io.sq;
    cmd[7] = (UINT32)((UINT64)io.sq >> 32);
    cmd[10] = io.qid | ((QUEUE_ENTRIES - 1) << 16);;
    cmd[11] = 1 | (io.qid << 16);
    if (Submit(admin, cmd) != 0) return false;

    return true;
}

bool Nvme::ReadBlocks(UINT32 nsid, UINT64 lba, UINT16 count, void *buffer) {
    UINT32 cmd[16];
    for (UINT32 i = 0; i < 16; i++) {
        cmd[i] = 0;
    }

    cmd[0] = 0x02 | ((UINT32)(nextCid++) << 16);
    cmd[1] = nsid;
    cmd[6] = (UINT32)(UINT64)buffer;
    cmd[7] = (UINT32)((UINT64)buffer >> 32);
    cmd[10] = (UINT32)lba;
    cmd[11] = (UINT32)(lba >> 32);
    cmd[12] = count - 1;

    return Submit(io, cmd) == 0;
}

bool Nvme::WriteBlocks(UINT32 nsid, UINT64 lba, UINT16 count, void *buffer) {
    UINT32 cmd[16];
    for (UINT32 i = 0; i < 16; i++) {
        cmd[i] = 0;
    }

    cmd[0] = 0x01 | ((UINT32)(nextCid++) << 16);
    cmd[1] = nsid;
    cmd[6] = (UINT32)(UINT64)buffer;
    cmd[7] = (UINT32)((UINT64)buffer >> 32);
    cmd[10] = (UINT32)lba;
    cmd[11] = (UINT32)(lba >> 32);
    cmd[12] = count - 1;

    return Submit(io, cmd) == 0;
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
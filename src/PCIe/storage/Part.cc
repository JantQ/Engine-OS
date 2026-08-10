#include "part.hh"
#include "Gpt.hh"
#include "../../Memory/Heap.hh"

static UINT8 *zeroBuf = 0;

bool Part::Mount(const UINT8 *typeGuid) {
    mounted = false;

    if (Gpt::Parse() != GPT_OK) return false;

    for (UINT32 i = 0; i < Gpt::count; i++) {
        bool match = true;
        for (UINT32 g = 0; g < 16; g++) {
            if (Gpt::parts[i].typeGuid[g] != typeGuid[g]) {
                match = false;
                break;
            }
        }
        if (!match) continue;

        baseLba = Gpt::parts[i].startingLba;
        sectors = Gpt::parts[i].endingLba - Gpt::parts[i].startingLba + 1;
        mounted = true;
        return true;
    }
    return false;
}

static bool Move(UINT64 lba, UINT32 count, UINT8 *buffer, bool write) {
    if ((UINTN)buffer & 0xFFF) return false;

    UINT32 perChunk =  4096 / Nvme::blockSize;

    for (UINT32 done = 0; done < count; done += perChunk) {
        UINT32 n = count - done;
        if (n > perChunk) n = perChunk;

        UINT8 *p = buffer + (UINTN)done * Nvme::blockSize;
        bool ok = write ? Nvme::WriteBlocks(Nvme::nsid, lba + done, (UINT16)n, p) 
                        : Nvme::ReadBlocks(Nvme::nsid, lba + done, (UINT16)n, p);
        
        if (!ok) return false;
    }
    return true;
}

bool Part::Read(UINT64 offsetLba, UINT32 count, void *buffer) {
    if (!InRange(offsetLba, count)) return false;
    return Move(baseLba + offsetLba, count, (UINT8*)buffer, false);
}

bool Part::Write(UINT64 offsetLba, UINT32 count, const void *buffer) {
    if (!InRange(offsetLba, count)) return false;
    return Move(baseLba + offsetLba, count, (UINT8*)buffer, true);
}

bool Part::Zero(UINT64 offsetLba, UINT64 count) {
    if (!zeroBuf) {
        zeroBuf = (UINT8*)Heap::Alloc(4096, 4096);
        
        if (!zeroBuf) return false;
        
        for (UINT32 i = 0; i < 4096; i++) {
            zeroBuf[i] = 0;
        }
    }

    UINT32 perChunk = 4096 / Nvme::blockSize;

    for (UINT64 done = 0; done < count; done += perChunk) {
        UINT32 n = (UINT32)(count - done);
        if (n > perChunk) n = perChunk;
        if (!Write(offsetLba + done, n, zeroBuf)) return false;
    }
    return true;
}
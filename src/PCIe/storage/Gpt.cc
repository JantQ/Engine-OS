#include "Gpt.hh"
#include "Nvme.hh"
#include "../../Memory/Heap.hh"
#include "../../Libs/Crc.hh"

static UINT8 *buffer = 0;
static UINT8 *array = 0;
static UINT32 arrayBytes = 0;

static bool MoveArray(UINT64 lba, bool write) {
    UINT32 blocksPerChunk = 4096 / Nvme::blockSize;

    for (UINT32 offset = 0; offset < arrayBytes; offset += 4096) {
        UINT64 chunkLba = lba + (UINT64)(offset / 4096) * blocksPerChunk;
        bool ok = write ? Nvme::WriteBlocks(Nvme::nsid, chunkLba, (UINT16)blocksPerChunk, array + offset)
                        : Nvme::ReadBlocks(Nvme::nsid, chunkLba, (UINT16)blocksPerChunk, array + offset);

        if (!ok) return false;
    }
    return true;
}

static bool WriteHeader(const GptHeader *header, UINT64 lba) {
    for (UINT32 i = 0; i < Nvme::blockSize; i++) buffer[i] = 0;
    for (UINT32 i = 0; i < sizeof(GptHeader); i++) buffer[i] = ((const UINT8*)header)[i];
    return Nvme::WriteBlocks(Nvme::nsid, lba, 1, buffer);
}

static bool RandBytes(UINT8 *out, UINT32 n) {
    for (UINT32 i = 0; i < n; i += 8) {
        UINT64 r = 0;
        UINT8 ok = 0;
        for(UINT32 t = 0; t < 16 && !ok; t++) {
            __asm__ __volatile__("rdrand %0; setc %1" : "=r"(r), "=qm"(ok));
        }

        if (!ok) return false;
        for (UINT32 b = 0; b < 8 && i + b < n; b++) {
            out[i + b] = (UINT8)(r >> (b * 8));
        }
    }
    return true;
}

static bool MakeGuid(UINT8 *guid) {
    if (!RandBytes(guid, 16)) return false;
    guid[7] = (guid[7] & 0x0F) | 0x40;
    guid[8] = (guid[8] & 0x3F) | 0x80;
    return true;
}

GptResult Gpt::Parse() {
    valid = false;
    count = 0;

    if (Nvme::nsid == 0) return GPT_NO_NAMESPACE;

    if (!buffer){
        buffer = (UINT8*)Heap::Alloc(4096, 4096);
    }
    if (!buffer) return GPT_NO_BUFFER;

    if (!Nvme::ReadBlocks(Nvme::nsid, 1, 1, buffer)) return GPT_HEADER_READ_FAILED;

    GptHeader *header = (GptHeader*)buffer;

    const char *signature = "EFI PART";
    for (UINT32 i = 0; i < 8; i++) {
        if (header->signature[i] != (UINT8)signature[i]) return GPT_BAD_SIGNATURE;
    }

    if (header->headerSize < 92 || header->headerSize > 512) return GPT_BAD_HEADER_SIZE;

    UINT32 stored = header->headerCrc32;
    header->headerCrc32 = 0;
    UINT32 computed = Crc::Crc32(buffer, header->headerSize);
    header->headerCrc32 = stored;
    if (computed != stored) return GPT_BAD_CRC;

    for (UINT32 b = 0; b < sizeof(GptHeader); b++) {
        ((UINT8*)&hdr)[b] = buffer[b];
    }

    firstUsable = header->firstUsableLba;
    lastUsable = header->lastUsableLba;
    UINT64 entryLba = header->partitionEntryLba;
    UINT32 entrySize = header->sizeOfPartitionEntry;
    UINT32 entryCount = header->numberOfPartitionEntries;

    if (entrySize < 128) return GPT_BAD_ENTRY_SIZE;

    UINT32 blocks = 4096 / Nvme::blockSize;
    if (!Nvme::ReadBlocks(Nvme::nsid, entryLba, (UINT16)blocks, buffer)) return GPT_ENTRY_READ_FAILED;

    UINT32 fit = 4096 / entrySize;
    if (entryCount > fit) entryCount = fit;

    for (UINT32 i = 0; i < entryCount && count < MAX_PARTS; i++) {
        GptEntry *entry = (GptEntry*)(buffer + i * entrySize);

        bool empty = true;
        for (UINT32 g = 0; g < 16; g++) {
            if (entry->typeGuid[g]) {
                empty = false;
                break;
            }
        }

        if (empty) continue;
        
        for (UINT32 b = 0; b < sizeof(GptEntry); b++) {
            ((UINT8*)&parts[count])[b] = ((UINT8*)entry)[b];
        }
        count++;
    }

    valid = true;
    return GPT_OK;
}

GptResult Gpt::CreatePartition(UINT64 startLba, UINT64 endLba, const UINT8 *typeGuid, const char *name) {
    GptResult r = Parse();
    if (r != GPT_OK) return r;

    if (startLba < firstUsable || endLba > lastUsable || startLba > endLba) {
        return GPT_RANGE_INVALID;
    }

    for (UINT32 i = 0; i < count; i++) {
        if (startLba <= parts[i].endingLba && parts[i].startingLba <= endLba) {
            return GPT_RANGE_OVERLAP;
        }
    }

    arrayBytes = hdr.numberOfPartitionEntries * hdr.sizeOfPartitionEntry;
    if (arrayBytes > 16384) return GPT_ARRAY_TOO_BIG;

    if (!array) array = (UINT8*)Heap::Alloc(16384, 4096);
    if (!array) return GPT_NO_BUFFER;

    if (!MoveArray(hdr.partitionEntryLba, false)) return GPT_ENTRY_READ_FAILED;

    if (Crc::Crc32(array, arrayBytes) != hdr.partitionArrayCrc32) {
        return GPT_BAD_ENTRY_CRC;
    }

    GptEntry *slot = 0;
    for (UINT32 i = 0; i < hdr.numberOfPartitionEntries; i++) {
        GptEntry *entry = (GptEntry*)(array + i * hdr.sizeOfPartitionEntry);
        bool free = true;
        for (UINT32 g = 0; g < 16; g++) {
            if (entry->typeGuid[g]) {
                free = false;
                break;
            }
        }
        if (free) {
            slot = entry;
            break;
        }
    }

    if (!slot) return GPT_NO_FREE_SLOT;

    for (UINT32 b = 0; b < sizeof(GptEntry); b++) {
        ((UINT8*)slot)[b] = 0;
    }

    for (UINT32 g = 0; g < 16; g++) {
        slot->typeGuid[g] = typeGuid[g];
    }

    if(!MakeGuid(slot->uinqueGuid)) return GPT_NO_RANDOM;

    slot->startingLba = startLba;
    slot->endingLba = endLba;
    slot->attributes = 0;

    for (UINT32 c = 0; c < 35 && name[c]; c++) {
        slot->name[c] = (UINT16)name[c];
    }

    UINT32 newArrayCrc = Crc::Crc32(array, arrayBytes);
    UINT64 arraySectors = arrayBytes / Nvme::blockSize;
    UINT64 backupArrayLba = hdr.alternateLba - arraySectors;

    if (!MoveArray(backupArrayLba, true)) return GPT_WRITE_FAILED;

    GptHeader backupHdr = hdr;
    backupHdr.myLba = hdr.alternateLba;
    backupHdr.alternateLba = hdr.myLba;
    backupHdr.partitionEntryLba = backupArrayLba;
    backupHdr.partitionArrayCrc32 = newArrayCrc;
    backupHdr.headerCrc32 = 0;
    backupHdr.headerCrc32 = Crc::Crc32(&backupHdr, backupHdr.headerSize);

    if (!WriteHeader(&backupHdr, hdr.alternateLba)) return GPT_WRITE_FAILED;
    
    if (!MoveArray(hdr.partitionEntryLba, true)) return GPT_WRITE_FAILED;

    hdr.partitionArrayCrc32 = newArrayCrc;
    hdr.headerCrc32 = 0;
    hdr.headerCrc32 = Crc::Crc32(&hdr, hdr.headerSize);

    return GPT_OK;
}
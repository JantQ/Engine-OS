#include "Gpt.hh"
#include "Nvme.hh"
#include "../../Memory/Heap.hh"
#include "../../Libs/Crc.hh"

static UINT8 *buffer = 0;

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
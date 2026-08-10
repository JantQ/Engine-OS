#include "EnFS.hh"
#include "../PCIe/storage/part.hh"
#include "../Libs/Crc.hh"
#include "../Libs/Str.hh"
#include "../Memory/Heap.hh"

static UINT8 *sectorBuffer = 0;
static UINT8 *tableBuffer = 0;

static const char *MAGIC = "ENFS0001";

static bool EnsureBuffers() {
    if (!sectorBuffer) sectorBuffer =(UINT8*)Heap::Alloc(4096, 4096);
    if (!tableBuffer) tableBuffer = (UINT8*)Heap::Alloc(4096, 4096);
    return sectorBuffer && tableBuffer;
}

static void Zero(UINT8 *p, UINT32 n) {
    for (UINT32 i = 0; i < n; i++) {
        p[i] = 0;
    }
}

static UINT64 TableSectors() {
    UINT32 blockSize = Part::BlockSize(); 
    UINT64 n = ENFS_TABLE_BYTES / blockSize;
    return n ? n : 1;
}

static UINT32 SuperCrc(EnFsSuper *super) {
    UINT32 saved = super->crc32;
    super->crc32 = 0;
    UINT32 c = Crc::Crc32(super, sizeof(EnFsSuper));
    super->crc32 = saved;
    return c;
}



static void TableToBuffer() {
    Zero(tableBuffer, ENFS_TABLE_BYTES);
    for (UINT32 i = 0; i < ENFS_MAX_FILES; i++) {
        UINT8 *dst = tableBuffer + i * sizeof(EnFsEntry);
        UINT8 *src = (UINT8*)&EnFS::files[i];
        for (UINT32 b = 0; b < sizeof(EnFsEntry); b++) {
            dst[b] = src[b];
        }
    }
}

static void BufferToTable() {
    for (UINT32 i = 0; i < ENFS_MAX_FILES; i++) {
        UINT8 *src = tableBuffer + i * sizeof(EnFsEntry);
        UINT8 *dst = (UINT8*)&EnFS::files[i];
        for (UINT32 b = 0; b < sizeof(EnFsEntry); b++) {
            dst[b] = src[b];
        }
    }
}



EnFsResult EnFS::Format() {
    if (!Part::mounted) return ENFS_NO_PARTITION;
    if (!EnsureBuffers()) return ENFS_NO_BUFFER;

    UINT64 tableSectors = TableSectors();

    Zero((UINT8*)&super, sizeof(EnFsSuper));
    for (UINT32 i = 0; i < 8; i++) {
        super.magic[i] = (UINT8)MAGIC[i];
    }

    super.version = 1;
    super.blockSize = Part::BlockSize();
    super.totalSectors = Part::sectors;
    super.dataStart = 1 + tableSectors;
    super.dataUsed = 0;
    super.maxFiles = ENFS_MAX_FILES;
    super.fileCount = 0;
    super.crc32 = SuperCrc(&super);

    for (UINT32 i = 0; i < ENFS_MAX_FILES; i++) {
        Zero((UINT8*)&files[i], sizeof(EnFsEntry));
    }

    mounted = true;
    return Flush();
}

EnFsResult EnFS::Flush() {
    if (!EnsureBuffers()) return ENFS_NO_BUFFER;

    super.crc32 = SuperCrc(&super);

    Zero(sectorBuffer, 4096);
    for (UINT32 b = 0; b < sizeof(EnFsSuper); b++) {
        sectorBuffer[b] = ((UINT8*)&super)[b];
    }
    if (!Part::Write(0, 1, sectorBuffer)) return ENFS_IO_FAILED;
    
    TableToBuffer();
    if (!Part::Write(1, (UINT32)TableSectors(), tableBuffer)) return ENFS_IO_FAILED;

    return ENFS_OK;
}

EnFsResult EnFS::Mount() {
    mounted = false;

    if (!Part::mounted) return ENFS_NO_PARTITION;
    if (!EnsureBuffers()) return ENFS_NO_BUFFER;

    if (!Part::Read(0, 1, sectorBuffer)) return ENFS_IO_FAILED;

    EnFsSuper *s = (EnFsSuper*)sectorBuffer;
    for (UINT32 i = 0; i < 8; i++) {
        if (s->magic[i] != (UINT8)MAGIC[i]) return ENFS_BAD_MAGIC;
    }
    if (SuperCrc(s) != s->crc32) return ENFS_BAD_CRC;

    for (UINT32 b = 0; b < sizeof(EnFsSuper); b++) {
        ((UINT8*)&super)[b] = sectorBuffer[b];
    }

    if (!Part::Read(1, (UINT32)TableSectors(), tableBuffer)) return ENFS_IO_FAILED;
    BufferToTable();

    mounted = true;
    return ENFS_OK;
}

EnFsEntry *EnFS::Find(const char *name) {
    if (!mounted) return 0;

    for (UINT32 i = 0; i < ENFS_MAX_FILES; i++) {
        if (files[i].flags != ENFS_USED) continue;
        if (Str::Equal(files[i].name, name)) return &files[i];
    }

    return 0;
}

EnFsResult EnFS::Remove(const char *name) {
    if (!mounted) return ENFS_NOT_MOUNTED;

    EnFsEntry *entry = Find(name);
    if (!entry) return ENFS_NOT_FOUND;

    Zero((UINT8*)entry, sizeof(EnFsEntry));
    super.fileCount--;
    return Flush();
}

EnFsResult EnFS::WriteFile(const char *name, const void *data, UINT64 bytes) {
    if (!mounted) return ENFS_NOT_MOUNTED;
    if (!EnsureBuffers()) return ENFS_NO_BUFFER;

    UINT32 nameLength = 0;
    while (name[nameLength]) nameLength++;
    if (nameLength >= ENFS_NAME_LENGHT) return ENFS_NAME_TOO_LONG;

    UINT32 blockSize = super.blockSize;
    UINT64 need = (bytes + blockSize - 1) / blockSize;
    if (need == 0) need = 1;

    EnFsEntry *entry = Find(name);

    if (entry) {
        UINT64 have = (entry->byteLenght + blockSize - 1) / blockSize;
        if (have < need) entry->startSector = 0;
    } else {
        for (UINT32 i = 0; i < ENFS_MAX_FILES; i++) {
            if (files[i].flags == ENFS_FREE) {
                entry = &files[i];
                break;
            }
        }

        if (!entry) return ENFS_NO_SLOT;

        Zero((UINT8*)entry, sizeof(EnFsEntry));
        for (UINT32 i = 0; i < nameLength; i++) {
            entry->name[i] = name[i];
        }
        entry->flags = ENFS_USED;
        super.fileCount++;
    }

    if (entry->startSector == 0) {
        if (super.dataStart + super.dataUsed + need > Part::sectors) return ENFS_NO_SPACE;
        entry->startSector = super.dataStart + super.dataUsed;
        super.dataUsed += need;
    }

    const UINT8 *src = (const UINT8*)data;
    UINT64 sector = entry->startSector;
    UINT64 left = bytes;

    while (left > 0) {
        UINT32 chunk = left > 4096 ? 4096 : (UINT32)left;

        Zero(sectorBuffer, 4096);
        for (UINT32 i = 0; i < chunk; i++) {
            sectorBuffer[i] = src[i];
        }

        UINT32 secs = (chunk + blockSize - 1) / blockSize;
        if (!Part::Write(sector, secs, sectorBuffer)) return ENFS_IO_FAILED;

        src += chunk;
        sector += secs;
        left -= chunk;
    }

    entry->byteLenght = bytes;
    return Flush();
}

EnFsResult EnFS::ReadFile(const char *name, void *buffer, UINT64 max, UINT64 *outLenght) {
    if (!mounted) return ENFS_NOT_MOUNTED;
    if (!EnsureBuffers()) return ENFS_NO_BUFFER;

    EnFsEntry *entry = Find(name);
    if (!entry) return ENFS_NOT_FOUND;

    UINT64 want = entry->byteLenght < max ? entry->byteLenght : max;
    UINT8 *dst = (UINT8*)buffer;
    UINT64 sector = entry->startSector;
    UINT64 done = 0;
    UINT32 blockSize = super.blockSize;

    while (done < want) {
        UINT64 remain = want - done;
        UINT32 chunk = remain > 4096 ? 4096 : (UINT32)remain;
        UINT32 secs = (chunk + blockSize - 1) / blockSize;

        if (!Part::Read(sector, secs, sectorBuffer)) return ENFS_IO_FAILED;
        for (UINT32 i = 0; i < chunk; i++) {
            dst[done + i] = sectorBuffer[i];
        }

        sector += secs;
        done += chunk;
    }

    if (outLenght) *outLenght = want;
    return ENFS_OK;
}
#pragma once

#include "../kernel.hh"

#define ENFS_MAX_FILES 64
#define ENFS_NAME_LENGHT 40
#define ENFS_TABLE_BYTES 4096

#define ENFS_FREE 0
#define ENFS_USED 1

struct __attribute__((packed)) EnFsSuper {
    UINT8 magic[8];
    UINT32 version;
    UINT32 blockSize;
    UINT64 totalSectors;
    UINT64 dataStart;
    UINT64 dataUsed;
    UINT32 maxFiles;
    UINT32 fileCount;
    UINT32 crc32;
};

struct __attribute__((packed)) EnFsEntry {
    char name[40];
    UINT64 startSector;
    UINT64 byteLenght;
    UINT8 flags;
    UINT8 pad[7];
};

enum EnFsResult {
    ENFS_OK = 0,
    ENFS_NO_PARTITION,
    ENFS_NO_BUFFER,
    ENFS_IO_FAILED,
    ENFS_BAD_MAGIC,
    ENFS_BAD_CRC,
    ENFS_NOT_MOUNTED,
    ENFS_NO_SPACE,
    ENFS_NO_SLOT,
    ENFS_NOT_FOUND,
    ENFS_NAME_TOO_LONG,
};

class EnFS {
    public:
        static inline bool mounted = false;
        static inline EnFsSuper super;
        static inline EnFsEntry files[64];

        static EnFsResult Format();
        static EnFsResult Mount();
        static EnFsResult Flush();

        static EnFsEntry *Find(const char *name);
        static EnFsResult Remove(const char *name);

        static EnFsResult WriteFile(const char *name, const void *data, UINT64 bytes);
        static EnFsResult ReadFile(const char *name, void *buffer, UINT64 max, UINT64 *outLenght);

        static const char *ResultName(EnFsResult result) {
            switch (result) {
                case ENFS_OK: return "ok";
                case ENFS_NO_PARTITION: return "no partition mounted";
                case ENFS_NO_BUFFER: return "alloc failed";
                case ENFS_IO_FAILED: return "io failed";
                case ENFS_BAD_MAGIC: return "not an enfs volume";
                case ENFS_BAD_CRC: return "bad superblock rcr";
                case ENFS_NOT_MOUNTED: return "not mounted";
                case ENFS_NO_SPACE: return "out of space";
                case ENFS_NO_SLOT: return "file table full";
                case ENFS_NOT_FOUND: return "not found";
                case ENFS_NAME_TOO_LONG: return "name too long";
            }
            return "unknow";
        }
};
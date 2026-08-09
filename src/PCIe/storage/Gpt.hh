#pragma once

#include "../../kernel.hh"

struct __attribute__((packed)) GptHeader {
    UINT8 signature[8];
    UINT32 revision;
    UINT32 headerSize;
    UINT32 headerCrc32;
    UINT32 reserved;
    UINT64 myLba;
    UINT64 alternateLba;
    UINT64 firstUsableLba;
    UINT64 lastUsableLba;
    UINT8 diskGuid[16];
    UINT64 partitionEntryLba;
    UINT32 numberOfPartitionEntries;
    UINT32 sizeOfPartitionEntry;
    UINT32 partitionArrayCrc32;
};

struct __attribute__((packed)) GptEntry {
    UINT8 typeGuid[16];
    UINT8 uinqueGuid[16];
    UINT64 startingLba;
    UINT64 endingLba;
    UINT64 attributes;
    UINT16 name[36];
};

enum GptResult {
    GPT_OK = 0,
    GPT_NO_NAMESPACE,
    GPT_NO_BUFFER,
    GPT_HEADER_READ_FAILED,
    GPT_BAD_SIGNATURE,
    GPT_BAD_HEADER_SIZE,
    GPT_BAD_CRC,
    GPT_BAD_ENTRY_SIZE,
    GPT_ENTRY_READ_FAILED,
    GPT_RANGE_INVALID,
    GPT_RANGE_OVERLAP,
    GPT_ARRAY_TOO_BIG,
    GPT_BAD_ENTRY_CRC,
    GPT_NO_FREE_SLOT,
    GPT_NO_RANDOM,
    GPT_WRITE_FAILED,
};

class Gpt {
    public: 
        static constexpr UINT32 MAX_PARTS = 32;

        static inline bool valid = false;
        static inline bool truncated = false;
        static inline UINT32 count = 0;
        static inline UINT64 firstUsable = 0;
        static inline UINT64 lastUsable = 0;
        static inline GptEntry parts[MAX_PARTS];
        static inline GptHeader hdr;

        static GptResult Parse();
        static GptResult CreatePartition(UINT64 startLba, UINT64 endLba, const UINT8 *typeGuid, const char *name);

        static const char *ResultName(GptResult result) {
            switch (result) {
                case GPT_OK: return "ok";
                case GPT_NO_NAMESPACE: return "no namespace";
                case GPT_NO_BUFFER: return "alloc failed";
                case GPT_HEADER_READ_FAILED: return "header read failed";
                case GPT_BAD_SIGNATURE: return "bad signature";
                case GPT_BAD_HEADER_SIZE: return "bad header size";
                case GPT_BAD_CRC: return "bad header crc";
                case GPT_BAD_ENTRY_SIZE: return "bad entry size";
                case GPT_ENTRY_READ_FAILED: return "entry read failed";
                case GPT_RANGE_INVALID: return "invalid range";
                case GPT_ARRAY_TOO_BIG: return "array too big";
                case GPT_BAD_ENTRY_CRC: return "bad entry crc";
                case GPT_NO_FREE_SLOT: return "no free slot";
                case GPT_NO_RANDOM: return "no random";
                case GPT_WRITE_FAILED: return "write failed";
            }
            return "unknown";
        }
};
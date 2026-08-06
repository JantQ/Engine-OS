#pragma once

#include "../../kernel.hh"
#include "../pci.hh"

#define NVME_CAP 0x00 // Capabilities
#define NVME_VS 0x08 // Version
#define NVME_INTMS 0x0C // interrupt set
#define NVME_INTMC 0x10 // interrupt clear
#define NVME_CC 0x14 // controller config
#define NVME_CSTS 0x1C // Controller status
#define NVME_AQA 0x24 // admin que attr
#define NVME_ASQ 0x28 // admin submission que
#define NVME_ACQ 0x30 // admin completion que

#define NVME_ADMIN_IDENTIFY 0x06


struct NvmeQueue {
    UINT8 *sq, *cq;
    UINT32 qid, sqTail, cqHead, phase, entries;
};



class Nvme {
    public:
        static inline UINT64 bar0 = 0;
        static inline UINT8 dstrd = 0;
        static inline UINT32 sqTail = 0;
        static inline UINT32 cqHead = 0;
        static inline UINT32 phase = 1;
        static inline UINT16 nextCid = 0;
        static inline NvmeQueue admin;
        static inline NvmeQueue io;

        static constexpr UINT32 QUEUE_ENTRIES = 64;

        static inline UINT8 *asq = 0;
        static inline UINT8 *acq = 0;


        static bool Init(PciDevice *device);
        static UINT16 submitAdmin(UINT32 *cmd);
        static bool Identify(UINT8 cns, UINT32 nsid, void *buffer);
        static UINT16 Submit(NvmeQueue &queue, UINT32 *cmd);
        static bool CreateIoQueues();
        static bool ReadBlocks(UINT32 nsid, UINT64 lba, UINT16 count, void *buffer);
        static bool WriteBlocks(UINT32 nsid, UINT64 lba, UINT16 count, void *buffer);

        static inline UINT32 Read32(UINT32 offset) {
            return *(volatile UINT32*)(bar0 + offset);
        }

        static inline void Write32(UINT32 offset, UINT32 value) {
            *(volatile UINT32*)(bar0 + offset) = value;
        }

        static inline UINT64 Read64(UINT32 offset) {
            return (UINT64)Read32(offset) | ((UINT64)Read32(offset + 4) << 32);
        }

        static inline void Write64(UINT32 offset, UINT64 value) {
            Write32(offset, (UINT32)value);
            Write32(offset + 4, (UINT32)(value >> 32));
        }

        static inline UINT32 DbStride() {
            return 4u << dstrd;
        }

        static inline UINT32 SqTailDb(UINT32 y) {
            return 0x1000 + (2 * y) * DbStride();
        }

        static inline UINT32 CqHeadDb(UINT32 y) {
            return 0x1000 + (2 * y + 1) * DbStride();
        }
    private:
        static bool WaitReady(bool wantReady, UINT64 timeoutMs);
        
        static void ZeroBytes(void *p, UINTN n) {
            volatile UINT8 *b = (volatile UINT8*)p;
            for (UINTN i = 0; i < n; i++) b[i] = 0;
        }
};


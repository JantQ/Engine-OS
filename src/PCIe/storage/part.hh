#pragma once

#include "../../kernel.hh"
#include "Nvme.hh"

class Part {
    public: 
        static inline bool mounted = false;
        static inline UINT64 baseLba = 0;
        static inline UINT64 sectors = 0;

        static bool Mount(const UINT8 *typeGuid);
        
        static bool Read(UINT64 offsetLba, UINT32 count, void *buffer);
        static bool Write(UINT64 offsetLba, UINT32 count, const void *buffer);

        static bool Zero(UINT64 offsetLba, UINT64 count);

        static UINT64 SizeBytes() {
            return sectors * Nvme::blockSize;
        }

        static UINT32 BlockSize() {
            return Nvme::blockSize;
        }
    
    private:
        static bool InRange(UINT64 offsetLba, UINT32 count) {
            if (!mounted || count == 0) return false;
            if (offsetLba >= sectors) return false;
            return (UINT64)count <= sectors - offsetLba;
        }
};
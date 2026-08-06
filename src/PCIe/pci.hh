#pragma once

#include "../kernel.hh"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

struct PciDevice {
    UINT8 bus, device, func;
    UINT16 vendorID, deviceID;
    UINT8 classCode, subClass, progIF;
};



class Pci {
    public:
        static UINT32 Read32(UINT8 bus, UINT8 device, UINT8 func, UINT8 off);
        static void Scan();
        static UINT64 ReadBar(UINT8 bus, UINT8 device, UINT8 func, UINT8 index);
        static void Write32(UINT8 bus, UINT8 device, UINT8 func, UINT8 off, UINT32 value);

        static constexpr UINT32 MAX_DEVICES = 64;
        static inline PciDevice devices[MAX_DEVICES];
        static inline UINT32 deviceCount = 0;

        static inline PciDevice *storage = 0;
        static PciDevice *FindClass(UINT8 classCode, UINT8 subClass);

        static void EnableMasterBus(UINT8 bus, UINT8 device, UINT8 func);
    
        private:
};
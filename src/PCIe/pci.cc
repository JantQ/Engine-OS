#include "pci.hh"
#include "../IO/io.hh"

static UINT32 ConfigAddress(UINT8 bus, UINT8 device, UINT8 func, UINT8 off) {
    return 0x80000000u | ((UINT32)bus << 16) | ((UINT32)device << 11) | ((UINT32)func << 8) | (off & 0xFC);
}

UINT32 Pci::Read32(UINT8 bus, UINT8 device, UINT8 func, UINT8 off) {
    UINT32 address = ConfigAddress(bus, device, func, off);

    IO::outl(PCI_CONFIG_ADDRESS, address);
    return IO::inl(PCI_CONFIG_DATA);
    
}

UINT64 Pci::ReadBar(UINT8 bus, UINT8 device, UINT8 func, UINT8 index) {
    UINT32 low = Read32(bus, device, func, 0x10 + index * 4);

    if (low & 0x1) return low & ~0x3u;

    UINT64 base = low & ~0xFu;

    if (((low >> 1) & 0x3) == 0x2) {
        UINT64 high = Read32(bus, device, func, 0x10 + (index + 1) * 4);
        base |= high << 32;
    }

    return base;
}

void Pci::Write32(UINT8 bus, UINT8 device, UINT8 func, UINT8 off, UINT32 value) {
    IO::outl(PCI_CONFIG_ADDRESS, ConfigAddress(bus, device, func, off));
    IO::outl(PCI_CONFIG_DATA, value);
}

void Pci::Scan() {
    deviceCount = 0;

    for (UINT32 bus = 0; bus < 256; bus++) {
        for (UINT32 device = 0; device < 32; device++) {
            
            if ((Read32(bus, device, 0, 0x00) & 0xFFFF) == 0xFFFF) continue;

            UINT8 headerType = (Read32(bus, device, 0, 0x0C) >> 16) & 0xFF;
            UINT32 funcCount = (headerType & 0x80) ? 8 : 1;

            for (UINT32 func = 0; func < funcCount; func++) {
                UINT32 id = Read32(bus, device, func, 0x00);
                if ((id & 0xFFFF) == 0xFFFF) continue;
                if (deviceCount >= MAX_DEVICES) return;

                UINT32 classReg = Read32(bus, device, func, 0x08);

                PciDevice &d = devices[deviceCount++];
                d.bus = bus;
                d.device = device;
                d.func = func;
                d.vendorID = id & 0xFFFF;
                d.deviceID = (id >> 16) & 0xFFFF;
                d.progIF = (classReg >> 8) & 0xFF;
                d.subClass = (classReg >> 16) & 0xFF;
                d.classCode = (classReg >> 24) & 0xFF;
            }
        }
    }
}

void Pci::EnableMasterBus(UINT8 bus, UINT8 device, UINT8 func) {
    UINT32 cmd = Read32(bus, device, func, 0x04) & 0xFFFF;
    cmd |= 0x07;
    Write32(bus, device, func, 0x04, cmd);
}


PciDevice *Pci::FindClass(UINT8 classCode, UINT8 subClass) {
   for (UINT32 i = 0; i < Pci::deviceCount; i++) {
      PciDevice &d = Pci::devices[i];

      if (d.classCode == classCode && d.subClass == subClass) {
         return &d;
      }
   }
   return 0;
}
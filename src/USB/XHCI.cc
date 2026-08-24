#include "XHCI.hh"
#include "../Memory/Paging.hh"
#include "../Serial/Serial.hh"
#include "../Text.hh"
#include "../Graphics/graphics.hh"
#include "../Clock/Clock.hh"
#include "../Memory/Heap.hh"
#include "../Console/Console.hh"

bool Xhci::Init(PciDevice *device) {
    if (!device) return false;
    if (device->progIF != 0x30) return false; // No xHCI

    Pci::EnableMasterBus(device->bus, device->device, device->func);

    bar0 = Pci::ReadBar(device->bus, device->device, device->func, 0);
    if (bar0 == 0) return false;

    if (!Paging::MapUc(bar0, 64 * 1024)) return false;

    UINT32 cap0 = CapRead32(XHCI_CAPLENGHT);
    capLenght = cap0 & 0xFF;
    hciVersion = (cap0 >> 16) & 0xFFFF;

    if (hciVersion == 0 || hciVersion == 0xFFFF) return false;

    UINT32 hcs1 = CapRead32(XHCI_HCSPARAMS1);
    maxSlots = hcs1 & 0xFF;
    maxIntrs = (hcs1 >> 8) & 0x7FF;
    maxPorts = (hcs1 >> 24) & 0xFF;

    UINT32 hcs2 = CapRead32(XHCI_HCSPARAMS2);
    scratchpadBuffers = (((hcs2 >> 21) & 0x1F) << 5) | ((hcs2 >> 27) & 0x1F);

    UINT32 hcc1 = CapRead32(XHCI_HCCPARAMS1);
    ac64 = (hcc1 & 0x1) != 0;
    contectSize = (hcc1 & 0x4) ? 64 : 32;
    xecpOffset = ((hcc1 >> 16) & 0xFFFF) * 4;

    opBase = bar0 + capLenght;
    runtimeBase = bar0 + (CapRead32(XHCI_RTSOFF) & ~0x1Fu);
    doorbellBase = bar0 + (CapRead32(XHCI_DBOFF) & ~0x3u);

    return true;
}

static inline UINT32 dumpY = 9;

void Xhci::Line(const char *label, UINT64 value, UINT32 digits) {
    Serial::Print(label);
    Serial::PrintHex(value, digits);
    Serial::Print("\n");

    Console::Print(label);
    Console::PrintHex(value, digits);
    Console::Print('\n');    
}


void Xhci::Dump() {
        Line("xHCI BAR0       ", bar0, 16);
    Line("  CAPLENGTH     ", capLenght, 2);
    Line("  HCIVERSION    ", hciVersion, 4);
    Line("  MaxSlots      ", maxSlots, 2);
    Line("  MaxIntrs      ", maxIntrs, 4);
    Line("  MaxPorts      ", maxPorts, 2);
    Line("  Scratchpads   ", scratchpadBuffers, 4);
    Line("  ContextSize   ", contectSize, 2);
    Line("  AC64          ", ac64 ? 1 : 0, 1);
    Line("  xECP offset   ", xecpOffset, 4);
    Line("  OpBase        ", opBase, 16);
    Line("  RuntimeBase   ", runtimeBase, 16);
    Line("  DoorbellBase  ", doorbellBase, 16);
}

void Xhci::WalkExtCaps() {
    if (xecpOffset == 0) {
        Serial::Print("xHCI: No extended capabilities\n");
        Console::Println("xHCI: no extended capabilities");
        return;
    }

    UINT32 offset = xecpOffset;

    for (UINT32 guard = 0; guard < 64; guard++) {
        UINT32 header = CapRead32(offset);

        if (header == 0xFFFFFFFF) break;

        UINT32 id = header & 0xFF;
        UINT32 next = (header >> 8) & 0xFF;

        Line("  ecp id        ", id, 2);
        Line("  ecp offset    ", offset, 4);

        if (id == XHCI_ECP_LEGACY) {
            legacyOffset = offset;

            UINT32 legsip = CapRead32(offset);
            UINT32 legctl = CapRead32(offset + 4);

            Line("    BIOS owned  ", (legsip >> 16) & 1, 1);
            Line("    OS owned    ", (legsip >> 24) & 1, 1);
            Line("    CTLSTS      ", legctl, 8);
        } else if (id == XHCI_ECP_PROTOCOL) {
            UINT32 rev = (header >> 16) & 0xFFFF;
            UINT32 name = CapRead32(offset + 4);
            UINT32 ports = CapRead32(offset + 8);

            UINT32 portOffset = ports & 0xFF;
            UINT32 portCount = (ports >> 8) & 0xFF;

            Line("    rev         ", rev, 4);
            Line("    name        ", name, 8);
            Line("    port first  ", portOffset, 2);
            Line("    port count  ", portCount, 2);

            for (UINT32 p = portOffset; p < portOffset + portCount && p < XHCI_MAX_PORTS; p++) {
                portProtocol[p] = (UINT8)(rev >> 8);
            }
        }

        if (next == 0) break;
        offset += next * 4;
    }
}

bool Xhci::TakeOwnership() {
    if (legacyOffset == 0) return true;

    UINT32 legsip = CapRead32(legacyOffset);

    CapWrite32(legacyOffset, legsip | XHCI_LEGSUP_OS_OWNED);

    if (legsip & XHCI_LEGSUP_BIOS_OWNED) {
        UINT64 deadline = Clock::Millis() + 1000;

        while (CapRead32(legacyOffset) & XHCI_LEGSUP_BIOS_OWNED) {
            if (Clock::Millis() > deadline) {
                Line("  handoff fail  ", CapRead32(legacyOffset), 8);
                return false;
            }
        }
    }

    CapWrite32(legacyOffset + 4, XHCI_LEGCTL_SMI_STATUS);

    Line("  legsup after  ", CapRead32(legacyOffset), 8);
    Line("  legctl after  ", CapRead32(legacyOffset + 4), 8);
    return true;
}

bool Xhci::HaltController() {
    if (OpRead32(XHCI_USBSTS) & XHCI_STS_HCH) return true;

    OpWrite32(XHCI_USBCMD, OpRead32(XHCI_USBCMD) & ~XHCI_CMD_RS);

    UINT64 deadline = Clock::Millis() + 32;
    
    while (!(OpRead32(XHCI_USBSTS) & XHCI_STS_HCH)) {
        if (Clock::Millis() > deadline) {
            Line("  halt TIMEOUT  ", OpRead32(XHCI_USBSTS), 8);
            return false;
        }
    }
    return true;    
}

bool Xhci::ResetController() {
    OpWrite32(XHCI_USBCMD, OpRead32(XHCI_USBCMD) | XHCI_CMD_HCRST);
    Clock::Delay(1);

    UINT64 deadline = Clock::Millis() + 1000;

    while (OpRead32(XHCI_USBCMD) & XHCI_CMD_HCRST) {
        if (Clock::Millis() > deadline) {
            Line("  hcrst TIMEOUT ", OpRead32(XHCI_USBCMD), 8);
            return false;
        }
    }

    while (OpRead32(XHCI_USBSTS) & XHCI_STS_CNR) {
        if (Clock::Millis() > deadline) {
            Line("  cnr TIMEOUT   ", OpRead32(XHCI_USBSTS), 8);
            return false;
        }
    }

    return true;
}

void Xhci::DumpOperational() {
    Line("  USBCMD        ", OpRead32(XHCI_USBCMD), 8);
    Line("  USBSTS        ", OpRead32(XHCI_USBSTS), 8);
    Line("  PAGESIZE      ", OpRead32(XHCI_PAGESIZE), 8);
    Line("  CONFIG        ", OpRead32(XHCI_CONFIG), 8);
    Line("  CRCR lo       ", OpRead32(XHCI_CRCR), 8);
    Line("  DCBAAP lo     ", OpRead32(XHCI_DCBAAP), 8);
}

static void Zero(void *p, UINT64 n) {
    UINT8 *b = (UINT8*)p;
    for (UINT64 i = 0; i < n; i++) {
        b[i] = 0;
    }
}

bool Xhci::StartController() {
    UINT32 dcbaaBytes = (maxSlots + 1) * 8;
    dcbaa = (UINT64*)Heap::Alloc(dcbaaBytes, 64);
    if (!dcbaa) return false;
    Zero(dcbaa, dcbaaBytes);

    if (scratchpadBuffers > 0) {
        UINT32 arrayBytes = scratchpadBuffers * 8;

        UINT64 *spArray = (UINT64*)Heap::Alloc(arrayBytes, 64);
        if (!spArray) return false;
        Zero(spArray, arrayBytes);

        for (UINT32 i = 0; i < scratchpadBuffers; i++) {
            void *page = Heap::Alloc(4096, 4096);
            if (!page) return false;
            Zero(page, 4096);
            spArray[i] = (UINT64)page;
        }

        dcbaa[0] = (UINT64)spArray;
    }

    UINT32 cmdBytes = XHCI_CMD_RING_TRBS * XHCI_TRB_SIZE;
    cmdRing = (XhciTrb*)Heap::Alloc(cmdBytes, 64);
    if (!cmdRing) return false;
    Zero(cmdRing, cmdBytes);

    XhciTrb &link = cmdRing[XHCI_CMD_RING_TRBS - 1];
    link.parameter = (UINT64)cmdRing;
    link.status = 0;
    link.control = (XHCI_TRB_TYPE_LINK << 10) | XHCI_TRB_TOGGLE;

    cmdEnqueue = 0;
    cmdCycle = 1;

    UINT32 evtBytes = XHCI_EVENT_RING_TRBS * XHCI_TRB_SIZE;
    eventRing = (XhciTrb*)Heap::Alloc(evtBytes, 64);
    if (!eventRing) return false;
    Zero(eventRing, evtBytes);

    erst = (XhciErstEntry*)Heap::Alloc(sizeof(XhciErstEntry), 64);
    if (!erst) return false;
    erst->ringSegmentBase = (UINT64)eventRing;
    erst->ringSegmentSize = XHCI_EVENT_RING_TRBS;
    erst->reserved = 0;

    eventDequeue = 0;
    eventCycle = 1;

    OpWrite32(XHCI_CONFIG, maxSlots);
    OpWrite64(XHCI_DCBAAP, (UINT64)dcbaa);
    OpWrite64(XHCI_CRCR, (UINT64)cmdRing | 1);

    RtWrite32(XHCI_IR0 + XHCI_ERSTSZ, 1);
    RtWrite64(XHCI_IR0 + XHCI_ERDP, (UINT64)eventRing);
    RtWrite64(XHCI_IR0 + XHCI_ERSTBA, (UINT64)erst);

    Line("  dcbaa         ", (UINT64)dcbaa, 16);
    Line("  cmdRing       ", (UINT64)cmdRing, 16);
    Line("  eventRing     ", (UINT64)eventRing, 16);
    Line("  erst          ", (UINT64)erst, 16);

    OpWrite32(XHCI_USBCMD, OpRead32(XHCI_USBCMD) | XHCI_CMD_RS);

    UINT64 deadline = Clock::Millis() + 100;

    while (OpRead32(XHCI_USBSTS) & XHCI_STS_HCH) {
        if (Clock::Millis() > deadline) {
            Line("  run TIMEOUT  ", OpRead32(XHCI_USBSTS), 8);
            return false;
        }
    }

    Line("  running USBSTS", OpRead32(XHCI_USBSTS), 8);
    return true;
}

void Xhci::DumpPorts() {
    for (UINT32 p = 1; p <= maxPorts; p++) {
        UINT32 sc = OpRead32(XHCI_PORTSC(p));

        if (!(sc & XHCI_PORT_CCS)) continue;

        Line("  PORT connected", p , 2);
        Line("    PORTSC      ", sc, 8);
        Line("    protocol    ", portProtocol[p], 1);
        Line("    speed       ", (sc >> 10) & 0xF, 1);
        Line("    enabled     ", (sc & XHCI_PORT_PED) ? 1 : 0, 1);
    }
}

void Xhci::PowerPorts() {
    bool powered = false;

    for (UINT32 p = 1; p <= maxPorts; p++) {
        UINT32 sc = OpRead32(XHCI_PORTSC(p));
        if (sc & XHCI_PORT_PP) continue;

        OpWrite32(XHCI_PORTSC(p), (sc & ~XHCI_PORTSC_RW1C) | XHCI_PORT_PP);
        powered = true;
    }

    if (powered) Clock::Delay(20);
}

bool Xhci::ResetPort(UINT32 port) {
    UINT32 sc = OpRead32(XHCI_PORTSC(port));
    if (!(sc & XHCI_PORT_CCS)) return false;

    OpWrite32(XHCI_PORTSC(port), (sc & ~XHCI_PORTSC_RW1C) | XHCI_PORT_PR);

    UINT64 deadline = Clock::Millis() + 100;

    while (!(OpRead32(XHCI_PORTSC(port)) & XHCI_PORT_PRC)) {
        if (Clock::Millis() > deadline) {
            Line("   reset TIMEOUT", OpRead32(XHCI_PORTSC(port)), 8);
            return false;
        }
    }

    sc = OpRead32(XHCI_PORTSC(port));
    OpWrite32(XHCI_PORTSC(port), (sc & ~XHCI_PORTSC_RW1C) | XHCI_PORT_PRC | XHCI_PORT_CSC);

    return (OpRead32(XHCI_PORTSC(port)) & XHCI_PORT_PED) != 0;
}

void Xhci::ResetAllPorts() {
    for (UINT32 p = 1; p <= maxPorts; p++) {
        UINT32 sc = OpRead32(XHCI_PORTSC(p));

        if (!(sc & XHCI_PORT_CCS)) continue;
        if (PortIsUsb3(p)) continue;

        Line("  reset port    ", p, 2);

        if (ResetPort(p)) {
            sc = OpRead32(XHCI_PORTSC(p));
            Line("   PORTSC     ", sc, 8);
            Line("   speed      ", (sc >> 10) & 0xF, 1);
            Line("   enabled    ", (sc & XHCI_PORT_PED) ? 1 : 0, 1);
        }
    }
}

void Xhci::PostCommand(UINT64 parameter, UINT32 status, UINT32 type) {
    volatile XhciTrb *trb = &cmdRing[cmdEnqueue];

    trb->parameter = parameter;
    trb->status = status;
    trb->control = (type << 10) | cmdCycle;

    cmdEnqueue++;

    if (cmdEnqueue == XHCI_CMD_RING_TRBS - 1) {
        volatile XhciTrb *link = &cmdRing[XHCI_CMD_RING_TRBS - 1];
        link->control = (link->control & ~XHCI_TRB_CYCLE) | cmdCycle;

        cmdEnqueue = 0;
        cmdCycle ^= 1;
    }

    Doorbell(0, 0);
}

bool Xhci::PollEvent(XhciTrb *out, UINT64 timeoutMs) {
    UINT64 deadline = Clock::Millis() + timeoutMs;

    for (;;) {
        volatile XhciTrb *trb = &eventRing[eventDequeue];

        if ((trb->control & XHCI_TRB_CYCLE) == eventCycle) {
            out->parameter = trb->parameter;
            out->status = trb->status;
            out->control = trb->control;

            eventDequeue++;
            if (eventDequeue == XHCI_EVENT_RING_TRBS) {
                eventDequeue = 0;
                eventCycle ^= 1;
            }

            RtWrite64(XHCI_IR0 + XHCI_ERDP, (UINT64)&eventRing[eventDequeue] | XHCI_ERDP_EHB);
            return true;
        }
        if (Clock::Millis() > deadline) return false;
    }
}

void Xhci::DrainEvents() {
    XhciTrb ev;
    UINT32 drained = 0;

    while (PollEvent(&ev, 5)) {
        drained++;
        if (drained > 64) break;
    }

    Line("  drained events", drained, 2);
}

bool Xhci::WaitCommand(XhciTrb *out, UINT64 timeoutMs) {
    UINT64 deadline = Clock::Millis() + timeoutMs;

    for (;;) {
        XhciTrb ev;

        if (PollEvent(&ev, 10)) {
            UINT32 type = (ev.control >> 10) & 0x3F;

            if (type == XHCI_TRB_TYPE_CMD_COMPLETION) {
                *out = ev;
                return true;
            }

            Line("  other event   ", type, 2);
            continue;
        }

        if (Clock::Millis() > deadline) return false;
    }
}

bool Xhci::EnableSlot(UINT32 *slotOut) {
    PostCommand(0, 0, XHCI_TRB_TYPE_ENABLE_SLOT);

    XhciTrb ev;
    if (!WaitCommand(&ev, 200)) {
        Line("  slot NO EVENT ", 0, 1);
        return false;
    }

    UINT32 code = (ev.status >> 24) & 0xFF;
    UINT32 slot = (ev.control >> 24) & 0xFF;

    Line("  completion    ", code, 2);
    Line("  slot id       ", slot, 2);

    if (code != 1) return false;

    *slotOut = slot;
    return true;
} 

static inline volatile UINT32 *Ctx(UINT8 *base, UINT32 index) {
    return (volatile UINT32*)(base + index * Xhci::contectSize);
}

static UINT32 MaxPacketForSpeed(UINT32 speed) {
    switch (speed) {
        case 2: return 8;
        case 1: return 8;
        case 3: return 64;
        case 4: return 512; 
        default: return 8;
    }
}

UINT32 Xhci::FirstConnectedPort(UINT32 wantSpeed) {
    for (UINT32 p = 1; p <= maxPorts; p++) {
        UINT32 sc = OpRead32(XHCI_PORTSC(p));

        if (!(sc & XHCI_PORT_CCS)) continue;
        if (!(sc & XHCI_PORT_PED)) continue;
        if (((sc >> 10) & 0xF) != wantSpeed) continue;

        return p;
    }
    return 0;
}

UINT32 Xhci::FirstEnabledPort(UINT32 *speedOut) {
    for (UINT32 p = 1; p <= maxPorts; p++) {
        UINT32 sc = OpRead32(XHCI_PORTSC(p));

        if (!(sc & XHCI_PORT_CCS)) continue;
        if (!(sc & XHCI_PORT_PED)) continue;
        
        *speedOut = (sc >> 10) & 0xF;
        return p;
    }
    return 0;
}

bool Xhci::AddressDevive(UINT32 slot, UINT32 port, UINT32 speed) {
    UINT32 devBytes = 32 * contectSize;
    deviceCtx = (UINT8*)Heap::Alloc(devBytes, 64);
    if (!deviceCtx) return false;
    Zero(deviceCtx, devBytes);

    dcbaa[slot] = (UINT64)deviceCtx;

    UINT32 ringBytes = XHCI_CMD_RING_TRBS * XHCI_TRB_SIZE;
    ep0Ring = (XhciTrb*)Heap::Alloc(ringBytes, 64);
    if (!ep0Ring) return false;
    Zero(ep0Ring, ringBytes);

    volatile XhciTrb *link = &ep0Ring[XHCI_CMD_RING_TRBS - 1];
    link->parameter = (UINT64)ep0Ring;
    link->status = 0;
    link->control = (XHCI_TRB_TYPE_LINK << 10) | XHCI_TRB_TOGGLE;

    ep0Enqueue = 0;
    ep0Cycle = 1;

    UINT32 inBytes = 33 * contectSize;
    inputCtx = (UINT8*)Heap::Alloc(inBytes, 64);
    if (!inputCtx) return false;
    Zero(inputCtx, inBytes);

    Ctx(inputCtx, 0)[1] = 0x3;

    volatile UINT32 *sc = Ctx(inputCtx, 1);
    sc[0] = (speed << 20) | (1u << 27);
    sc[1] = (port << 16);

    volatile UINT32 *ep = Ctx(inputCtx, 2);
    ep[1] = (MaxPacketForSpeed(speed) << 16) | (XHCI_EP_TYPE_CONTROL << 3) | (3u << 1);
    ep[2] = (UINT32)((UINT64)ep0Ring | 1);
    ep[3] = (UINT32)(((UINT64)ep0Ring) >> 32);
    ep[4] = 8;

    volatile XhciTrb *trb = &cmdRing[cmdEnqueue];
    trb->parameter = (UINT64)inputCtx;
    trb->status = 0;
    trb->control = (XHCI_TRB_TYPE_ADDRESS_DEVICE << 10) | (slot << 24) | cmdCycle;

    cmdEnqueue++;
    if (cmdEnqueue == XHCI_CMD_RING_TRBS - 1) {
        volatile XhciTrb *l = &cmdRing[XHCI_CMD_RING_TRBS - 1];
        l->control = (l->control & ~XHCI_TRB_CYCLE) | cmdCycle;
        cmdEnqueue = 0;
        cmdCycle ^= 1;
    }

    Doorbell(0, 0);

    XhciTrb ev;
    if (!WaitCommand(&ev, 500)) {
        Line("  addr NO EVENT", 0, 1);
        return false;
    }

    UINT32 code = (ev.status >> 24) & 0xFF;
    Line("  addr completion  ", code, 2);

    if (code != 1) return false;

    UINT32 addr = Ctx(deviceCtx, 0)[3] & 0xFF;
    Line("  usb address     ", addr, 2);

    deviceSlot = slot;
    return true;
}

static void PushEp0(UINT64 param, UINT32 status, UINT32 control) {
    volatile XhciTrb *trb = &Xhci::ep0Ring[Xhci::ep0Enqueue];

    trb->parameter = param;
    trb->status = status;
    trb->control = control | Xhci::ep0Cycle;

    Xhci::ep0Enqueue++;

    if (Xhci::ep0Enqueue == XHCI_CMD_RING_TRBS - 1) {
        volatile XhciTrb *l = &Xhci::ep0Ring[XHCI_CMD_RING_TRBS - 1];
        l->control = (l->control & ~XHCI_TRB_CYCLE) | Xhci::ep0Cycle;

        Xhci::ep0Enqueue = 0;
        Xhci::ep0Cycle ^= 1;
    }
}

bool Xhci::ControlIn(UINT32 slot, UINT64 setup, void *buffer, UINT32 length) {
    PushEp0(setup, 8, (XHCI_TRB_TYPE_SETUP << 10) | (3u << 16) | (1u << 6));

    if (length > 0) {
        PushEp0((UINT64)buffer, length, (XHCI_TRB_TYPE_DATA << 10) | (1u << 16));
    }

    PushEp0(0, 0, (XHCI_TRB_TYPE_STATUS << 10) | (1u << 5));

    Doorbell(slot, 1);

    UINT64 deadline = Clock::Millis() + 500;

    for (;;) {
        XhciTrb ev;

        if (PollEvent(&ev, 10)) {
            UINT32 type = (ev.control >> 10) & 0x3F;

            if (type == XHCI_TRB_TYPE_TRANSFER_EVENT) {
                UINT32 code = (ev.status >> 24) & 0xFF;
                Line("  xfer complete ", code, 2);
                return code == 1 || code == 13;
            }

            Line("  other event   ", type, 2);
            continue;
        }

        if (Clock::Millis() > deadline) {
            Line("  xfer NO EVENT ", 0, 1);
            return false;
        }
    }
}

bool Xhci::GetDeviceDescriptor(UINT32 slot) {
    UINT8 *buffer = (UINT8*)Heap::Alloc(64, 64);
    if (!buffer) return false;
    Zero(buffer, 64);

    UINT64 setup = 0x80ull | (6ull << 8) | (0x0100ull << 16) | (0ull << 32) | (8ull << 48);
    
    if (!ControlIn(slot, setup, buffer, 8)) return false;
    
    UINT32 mps = buffer[7];
    Line("  bMaxPacket0    ", mps, 2);

    if (mps != 8) {
        if (!EvaluateContext(slot, mps)) return false;
    }

    Zero(buffer, 64);
    setup = 0x80ull | (6ull << 8) | (0x0100ull << 16) | (0ull << 32) | (18ull << 48);
    
    if (!ControlIn(slot, setup, buffer, 18)) return false;
    Line("  bLength       ", buffer[0], 2);
    Line("  bDescType     ", buffer[1], 2);
    Line("  bcdUSB        ", buffer[2] | (buffer[3] << 8), 4);
    Line("  bDeviceClass  ", buffer[4], 2);
    Line("  bMaxPacket0   ", buffer[7], 2);
    Line("  idVendor      ", buffer[8] | (buffer[9] << 8), 4);
    Line("  idProduct     ", buffer[10] | (buffer[11] << 8), 4);
    Line("  bNumConfigs   ", buffer[17], 2);

    return true;
}

bool Xhci::EvaluateContext(UINT32 slot, UINT32 maxPacket) {
    Zero(inputCtx, 33 * contectSize);

    Ctx(inputCtx, 0)[1] = 0x2;

    volatile UINT32 *ep = Ctx(inputCtx, 2);
    ep[1] = (maxPacket << 16) | (XHCI_EP_TYPE_CONTROL << 3) | (3u << 1);

    volatile XhciTrb *trb = &cmdRing[cmdEnqueue];
    trb->parameter = (UINT64)inputCtx;
    trb->status    = 0;
    trb->control   = (XHCI_TRB_TYPE_EVAL_CONTEXT << 10) | (slot << 24) | cmdCycle;

    cmdEnqueue++;
    if (cmdEnqueue == XHCI_CMD_RING_TRBS - 1) {
        volatile XhciTrb *l = &cmdRing[XHCI_CMD_RING_TRBS - 1];
        l->control = (l->control & ~XHCI_TRB_CYCLE) | cmdCycle;
        cmdEnqueue = 0;
        cmdCycle  ^= 1;
    }

    Doorbell(0, 0);

    XhciTrb ev;
    if (!WaitCommand(&ev, 200)) return false;

    UINT32 code = (ev.status >> 24) & 0xFF;
    Line("  eval complete ", code, 2);
    return code == 1;
}

bool Xhci::GetConfigDescriptor(UINT32 slot) {
    UINT8 *buffer = (UINT8*)Heap::Alloc(512, 64);
    if (!buffer) return false;
    Zero(buffer, 512);

    UINT64 setup = 0x80ull | (6ull << 8) | (0x0200ull << 16) | (0ull << 32) | (9ull << 48);

    if (!ControlIn(slot, setup, buffer, 9)) return false;

    UINT32 total = buffer[2] | (buffer[3] << 8);
    Line("  wTotalLength  ", total, 4);
    Line("  bNumInterfaces ", buffer[4], 2);

    if (total > 512) total = 512;

    Zero(buffer, 512);
    setup = 0x80ull | (6ull << 8) | (0x0200ull << 16) | (0ull << 32) | ((UINT64)total << 48);

    if (!ControlIn(slot, setup, buffer, total)) return false;

    configValue = buffer[5];

    UINT32 i = 0;
    UINT32 iface = 0xFF;
    bool isBootKeyboard = false;

    while (i + 1 < total) {
        UINT32 lenght = buffer[i];
        UINT32 type = buffer[i + 1];

        if (lenght == 0) break;

        if (type == 4) {
            iface = buffer[i + 2];
            isBootKeyboard = (buffer[i + 5] == 3) // class HID
                          && (buffer[i + 6] == 1) // subclass Boot
                          && (buffer[i + 7] == 1); // protocol Keyboard
            
            Line(" interface    ", iface, 2);
            Line("    class     ", buffer[i + 5], 2);
            Line("    subclass  ", buffer[i + 6], 2);
            Line("    protocol  ", buffer[i + 7], 2); 
        } else if (type == 5) {// ENDPOINT 
            UINT32 addr = buffer[i + 2];
            UINT32 attr = buffer[i + 3];
            UINT32 mps = buffer[i + 4] | (buffer[i + 5] << 8);

            Line("   endpoint   ", addr, 2);

            bool isIn = (addr & 0x80) != 0;
            bool isInterrupt = (attr & 0x3) == 3;

            if (isBootKeyboard && isIn && isInterrupt && hidEndpoint == 0) {
                hidInterface = iface;
                hidEndpoint = addr & 0xF;
                hidMaxPacket = mps;
                hidInterval = buffer[i + 6];
            }
        }

        i += lenght;
    }

    Line("  HID iface     ", hidInterface, 2);
    Line("  HID endpoint  ", hidEndpoint, 2);
    Line("  HID maxpacket ", hidMaxPacket, 4);
    Line("  HID interval  ", hidInterval, 2);

    return hidEndpoint != 0;
}

bool Xhci::ControlOut(UINT32 slot, UINT64 setup) {
    PushEp0(setup, 8, (XHCI_TRB_TYPE_SETUP << 10) | (0u << 16) | (1u << 6));

    PushEp0(0, 0, (XHCI_TRB_TYPE_STATUS << 10) | (1u << 16) | (1u << 5));

    Doorbell(slot, 1);

    UINT64 deadline = Clock::Millis() + 500;

    for (;;) {
        XhciTrb ev;

        if (PollEvent(&ev, 10)) {
            if (((ev.control >> 10) & 0x3F) == XHCI_TRB_TYPE_TRANSFER_EVENT) {
                UINT32 code = (ev.status >> 24) & 0xFF;
                Line("  ctrl out     ", code, 2);
                return code == 1 || code == 13;
            }
            continue;
        }

        if (Clock::Millis() > deadline) return false;
    }
}

bool Xhci::SetConfiguration(UINT32 slot) {
    UINT64 setup = 0x00ull | (9ull << 8) | ((UINT64)configValue << 16);
    return ControlOut(slot, setup);
}

bool Xhci::SetBootProtocol(UINT32 slot) {
    UINT64 setup = 0x21ull | (0x0Bull << 8) | (0ull << 16) | ((UINT64)hidInterface << 32);

    if (!ControlOut(slot, setup)) return false;

    setup = 0x21ull | (0x0Aull << 8) | (0ull << 16) | ((UINT64)hidInterface << 32);

    ControlOut(slot, setup);
    return true;
}

static UINT32 EncodeInterval(UINT32 speed, UINT32 bInterval) {
    if (speed >= 3) return bInterval ? bInterval - 1 : 0;

    UINT32 ms = bInterval ? bInterval : 1;
    UINT32 exp = 0;
    while((1u << (exp + 1)) <= ms && exp < 7) exp++;
    return 3 + exp;
}

bool Xhci::ConfigureHidEndpoint(UINT32 slot, UINT32 speed) {
    hidDci = hidEndpoint * 2 + 1;
    Line("  hid DCI      ", hidDci, 2);

    UINT32 ringBytes = XHCI_CMD_RING_TRBS * XHCI_TRB_SIZE;
    hidRing = (XhciTrb*)Heap::Alloc(ringBytes, 64);
    if (!hidRing) return false;
    Zero(hidRing, ringBytes);

    volatile XhciTrb *link = &hidRing[XHCI_CMD_RING_TRBS - 1];
    link->parameter = (UINT64)hidRing;
    link->status = 0;
    link->control = (XHCI_TRB_TYPE_LINK << 10) | XHCI_TRB_TOGGLE;

    hidEnqueue = 0;
    hidCycle = 1;

    hidReport = (UINT8*)Heap::Alloc(64, 64);
    if (!hidReport) return false;
    Zero(hidReport, 64);

    Zero(inputCtx, 33 * contectSize);
    Ctx(inputCtx, 0)[1] = (1u << 0) | (1u << hidDci);

    volatile UINT32 *sc = Ctx(inputCtx, 1);
    sc[0] = (speed << 20) | (hidDci << 27);
    sc[1] = Ctx(deviceCtx, 0)[1];

    volatile UINT32 *ep = Ctx(inputCtx, hidDci + 1);
    ep[0] = EncodeInterval(speed, hidInterval) << 16;
    ep[1] = (hidMaxPacket << 16) | (XHCI_EP_TYPE_INTR_IN << 3) | (3u << 1);
    ep[2] = (UINT32)((UINT64)hidRing | 1);
    ep[3] = (UINT32)(((UINT64)hidRing) >> 32);
    ep[4] = hidMaxPacket | (hidMaxPacket << 16);

    volatile XhciTrb *trb = &cmdRing[cmdEnqueue];
    trb->parameter = (UINT64)inputCtx;
    trb->status = 0;
    trb->control = (XHCI_TRB_TYPE_CONFIG_EP << 10) | (slot << 24) | cmdCycle;

    cmdEnqueue++;
    if (cmdEnqueue == XHCI_CMD_RING_TRBS - 1) {
        volatile XhciTrb *l = &cmdRing[XHCI_CMD_RING_TRBS - 1];
        l->control = (l->control & ~XHCI_TRB_CYCLE) | cmdCycle;
        cmdEnqueue = 0;
        cmdCycle ^= 1;
    }

    Doorbell(0, 0);

    XhciTrb ev;
    if (!WaitCommand(&ev, 500)) return false;

    UINT32 code = (ev.status >> 24) & 0xFF;
    Line("  config ep    ", code, 2);
    return code == 1;
}

void Xhci::QueueHidRead(UINT32 slot) {
    volatile XhciTrb *trb = &hidRing[hidEnqueue];

    trb->parameter = (UINT64)hidReport;
    trb->status = hidMaxPacket;
    trb->control = (XHCI_TRB_TYPE_NORMAL << 10) | (1u << 5) | hidCycle;

    hidEnqueue++;
    if (hidEnqueue == XHCI_CMD_RING_TRBS - 1) {
        volatile XhciTrb *l = &hidRing[XHCI_CMD_RING_TRBS - 1];
        l->control = (l->control & ~XHCI_TRB_CYCLE) | hidCycle;
        hidEnqueue = 0;
        hidCycle ^= 1;
    }

    Doorbell(slot, hidDci);
}

bool Xhci::PollHidReport(UINT32 timoutMs) {
    UINT64 deadline = Clock::Millis() + timoutMs;

    for (;;) {
        XhciTrb ev;

        if (PollEvent(&ev, 5)) {
            if (((ev.control >> 10) & 0x3F) == XHCI_TRB_TYPE_TRANSFER_EVENT) {
                UINT32 code = (ev.status >> 24) & 0xFF;
                return code == 1 || code == 13;
            }
            continue;
        }

        if (Clock::Millis() > deadline) return false;
    }
}
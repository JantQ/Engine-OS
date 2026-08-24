#pragma once

#include "../kernel.hh"
#include "../PCIe/pci.hh"

#define XHCI_CAPLENGHT 0x00
#define XHCI_HCSPARAMS1 0x04
#define XHCI_HCSPARAMS2 0x08
#define XHCI_HCSPARAMS3 0x0C
#define XHCI_HCCPARAMS1 0x10
#define XHCI_DBOFF 0x14
#define XHCI_RTSOFF 0x18
#define XHCI_ECP_LEGACY 1
#define XHCI_ECP_PROTOCOL 2

#define XHCI_USBCMD    0x00
#define XHCI_USBSTS    0x04
#define XHCI_PAGESIZE  0x08
#define XHCI_DNCTRL    0x14
#define XHCI_CRCR      0x18
#define XHCI_DCBAAP    0x30
#define XHCI_CONFIG    0x38
#define XHCI_PORTSC(p) (0x400 + ((p) - 1) * 0x10)

#define XHCI_CMD_RS    (1u << 0)
#define XHCI_CMD_HCRST (1u << 1)
#define XHCI_CMD_INTE  (1u << 2)
#define XHCI_CMD_HSEE  (1u << 3)

#define XHCI_STS_HCH   (1u << 0)
#define XHCI_STS_HSE   (1u << 2)
#define XHCI_STS_EINT  (1u << 3)
#define XHCI_STS_PCD   (1u << 4)
#define XHCI_STS_CNR   (1u << 11)
#define XHCI_STS_HCE   (1u << 12)

#define XHCI_LEGSUP_BIOS_OWNED (1u << 16)
#define XHCI_LEGSUP_OS_OWNED   (1u << 24)
#define XHCI_LEGCTL_SMI_STATUS 0xE01F0000u

#define XHCI_MAX_PORTS 256

#define XHCI_TRB_SIZE        16
#define XHCI_CMD_RING_TRBS   256
#define XHCI_EVENT_RING_TRBS 256

#define XHCI_IR0     0x20
#define XHCI_IMAN    0x00
#define XHCI_IMOD    0x04
#define XHCI_ERSTSZ  0x08
#define XHCI_ERSTBA  0x10
#define XHCI_ERDP    0x18

#define XHCI_TRB_TYPE_LINK 6
#define XHCI_TRB_CYCLE     (1u << 0)
#define XHCI_TRB_TOGGLE    (1u << 1)

#define XHCI_PORT_CCS  (1u << 0)
#define XHCI_PORT_PED  (1u << 1)
#define XHCI_PORT_PR   (1u << 4)
#define XHCI_PORT_PP   (1u << 9)
#define XHCI_PORTSC_RW1C 0x00FE0002u
#define XHCI_PORT_CSC (1u << 17)
#define XHCI_PORT_PEC (1u << 18)
#define XHCI_PORT_PRC (1u << 21)

#define XHCI_TRB_TYPE_NORMAL     1
#define XHCI_TRB_TYPE_SETUP  2
#define XHCI_TRB_TYPE_DATA   3
#define XHCI_TRB_TYPE_STATUS 4
#define XHCI_TRB_TYPE_ENABLE_SLOT      9
#define XHCI_TRB_TYPE_ADDRESS_DEVICE  11
#define XHCI_TRB_TYPE_CONFIG_EP 12
#define XHCI_TRB_TYPE_EVAL_CONTEXT 13
#define XHCI_TRB_TYPE_TRANSFER_EVENT  32
#define XHCI_TRB_TYPE_CMD_COMPLETION  33
#define XHCI_TRB_TYPE_PORT_STATUS     34

#define XHCI_ERDP_EHB (1u << 3)
#define XHCI_EP_TYPE_CONTROL 4
#define XHCI_EP_TYPE_INTR_IN     7

struct XhciTrb {
    UINT64 parameter;
    UINT32 status;
    UINT32 control;
};

struct XhciErstEntry {
    UINT64 ringSegmentBase;
    UINT32 ringSegmentSize;
    UINT32 reserved;
};

class Xhci {
    public:
        static inline UINT64 bar0 = 0;
        static inline UINT64 opBase = 0;
        static inline UINT64 runtimeBase = 0;
        static inline UINT64 doorbellBase = 0;
        static inline UINT64 *dcbaa = 0;

        static inline UINT32 capLenght = 0;
        static inline UINT32 hciVersion = 0;
        static inline UINT32 legacyOffset = 0;
        static inline UINT32 maxSlots = 0;
        static inline UINT32 maxIntrs = 0;
        static inline UINT32 maxPorts = 0;
        static inline UINT32 scratchpadBuffers = 0;
        static inline UINT32 contectSize = 32;
        static inline UINT32 xecpOffset = 0;
        static inline UINT32 cmdEnqueue;
        static inline UINT32 cmdCycle = 1;
        static inline UINT32 eventDequeue = 0;
        static inline UINT32 eventCycle = 1;
        static inline UINT32 ep0Enqueue = 0;
        static inline UINT32 ep0Cycle = 1;
        static inline UINT32 deviceSlot = 0;
        static inline UINT32 configValue = 0;
        static inline UINT32 hidInterface = 0xFF;
        static inline UINT32 hidEndpoint = 0;
        static inline UINT32 hidMaxPacket = 0;
        static inline UINT32 hidInterval = 0;
        static inline UINT32 hidEnqueue = 0;
        static inline UINT32 hidCycle = 1;
        static inline UINT32 hidDci = 0;

        static inline UINT8 portProtocol[XHCI_MAX_PORTS] = {};
        static inline UINT8 *inputCtx = 0;
        static inline UINT8 *deviceCtx = 0;
        static inline UINT8 *hidReport = 0;

        static inline XhciTrb *cmdRing = 0;
        static inline XhciTrb *eventRing = 0;
        static inline XhciTrb *ep0Ring = 0;
        static inline XhciTrb *hidRing = 0;

        static inline XhciErstEntry *erst = 0;

        static inline bool ac64 = false;


        
        static void Line(const char *label, UINT64 value, UINT32 digits);
        static void Dump();
        static void WalkExtCaps();
        static void DumpOperational();
        static void DumpPorts();
        static void PowerPorts();
        static void ResetAllPorts();
        static void PostCommand(UINT64 parameter, UINT32 status, UINT32 type);
        static void DrainEvents();
        static void QueueHidRead(UINT32 slot);


        static bool Init(PciDevice *device);
        static bool TakeOwnership();
        static bool HaltController();
        static bool ResetController();
        static bool StartController();
        static bool ResetPort(UINT32 port);
        static bool PollEvent(XhciTrb *out, UINT64 timeoutMs);
        static bool WaitCommand(XhciTrb *out, UINT64 timeoutMs);
        static bool EnableSlot(UINT32 *slotOut);
        static bool AddressDevive(UINT32 slot, UINT32 port, UINT32 speed);
        static bool ControlIn(UINT32 slot, UINT64 setup, void *buffer, UINT32 length);
        static bool GetDeviceDescriptor(UINT32 slot);
        static bool EvaluateContext(UINT32 slot, UINT32 maxPacket);
        static bool GetConfigDescriptor(UINT32 slot);
        static bool ControlOut(UINT32 slot, UINT64 setup);
        static bool SetConfiguration(UINT32 slot);
        static bool SetBootProtocol(UINT32 slot);
        static bool ConfigureHidEndpoint(UINT32 slot, UINT32 speed);
        static bool PollHidReport(UINT32 timoutMs);

        
        static UINT32 FirstConnectedPort(UINT32 wantSpeed);
        static UINT32 FirstEnabledPort(UINT32 *speedOut);

        static inline UINT32 CapRead32(UINT32 offset) {
            UINT32 v;
            __asm__ __volatile__("movl %1, %0" : "=r"(v) : "m"(*(volatile UINT32*)(bar0 + offset)));
            return v;
        }

        static inline UINT32 OpRead32(UINT32 offset) {
            UINT32 v;
            __asm__ __volatile__("movl %1, %0" : "=r"(v) : "m"(*(volatile UINT32*)(opBase + offset)));
            return v;
        }

        static inline UINT32 RtRead32(UINT32 offset) {
            UINT32 v;
            __asm__ __volatile__("movl %1, %0" : "=r"(v) : "m"(*(volatile UINT32*)(runtimeBase + offset)));
            return v;
        }


        static inline void CapWrite32(UINT32 offset, UINT32 value) {
            __asm__ __volatile__("movl %1, %0" : "=m"(*(volatile UINT32*)(bar0 + offset)) : "r"(value));
        }

        static inline void OpWrite32(UINT32 offset, UINT32 value) {
            __asm__ __volatile__("movl %1, %0" : "=m"(*(volatile UINT32*)(opBase + offset)) : "r"(value));
        }

        static inline void OpWrite64(UINT32 offset, UINT64 value) {
            __asm__ __volatile__("movq %1, %0" : "=m"(*(volatile UINT64*)(opBase + offset)) : "r"(value));
        }

        static inline void RtWrite32(UINT32 offset, UINT32 value) {
            __asm__ __volatile__("movl %1, %0" : "=m"(*(volatile UINT32*)(runtimeBase + offset)) : "r"(value));
        }

        static inline void RtWrite64(UINT32 offset, UINT64 value) {
            __asm__ __volatile__("movq %1, %0" : "=m"(*(volatile UINT64*)(runtimeBase + offset)) : "r"(value));
        }

        static inline void Doorbell(UINT32 slot, UINT32 value) {
            __asm__ __volatile__("movl %1, %0" : "=m"(*(volatile UINT32*)(doorbellBase + slot * 4)) : "r"(value) : "memory");
        }


        static inline bool PortIsUsb3(UINT32 port) {
            return port < XHCI_MAX_PORTS && portProtocol[port] == 3;
        }

        static inline bool PortIsUsb2(UINT32 port)  {
            return port < XHCI_MAX_PORTS && portProtocol[port] == 2;
        }
};
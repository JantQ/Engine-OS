#include "kernel.hh"
#include "Clock/Clock.hh"
#include "Graphics/graphics.hh"
#include "Text.hh"
#include "input/keyboard.hh"

extern "C" UINT32 init_kernel(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    Clock::StartClock(SystemTable);

    EFI_GUID gopGuid = {0x9042a9de, 0x23dc, 0x4a38, 
        {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    SystemTable->BootServices->LocateProtocol(&gopGuid, 0, (void**)&gop);
    SystemTable->ConOut->Reset(SystemTable->ConOut, true);

    // Memory  
    UINTN mapSize = 0;
    EFI_MEMORY_DESCRIPTOR *memoryMap = 0;
    UINTN mapKey;
    UINTN descriptorSize;
    UINT32 descriptorVersion;

    SystemTable->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);

    mapSize += 2 * descriptorSize;

    SystemTable->BootServices->AllocatePool(EfiLoaderData, mapSize, (void**)&memoryMap);

    EFI_STATUS MemoryStatus = SystemTable->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);
    
    
    // Graphics
    UINT32 version = gop->Mode->Info->Version;
    UINT32 *framebuffer = (UINT32*)gop->Mode->FrameBufferBase;
    UINT32 width = gop->Mode->Info->HorizontalResolution;
    UINT32 height = gop->Mode->Info->VerticalResolution;
    UINT32 stride = gop->Mode->Info->PixelsPerScanLine;

    

    // EXIT UEFI
    
    EFI_STATUS BootStatus = SystemTable->BootServices->ExitBootServices(ImageHandle ,mapKey);
    
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            framebuffer[x + y * stride] = Graphics::encodeColor(0, 255, 0);
            UINT32 existing = framebuffer[x + y * stride];
            framebuffer[(x + 100) + (y * stride + 100)] = Graphics::blendColor(existing, 0x00FF0000, 128);
        }
    }

    Text::DrawString(framebuffer, stride, 100, 200, "Hello Engine OS! \x01\x02\x03", Graphics::encodeColor(255, 0, 0));
    
    int x = 100;

    while (1) {
        char c = Keyboard::ReadChar();
        
        if (c != 0) {
            Text::DrawChar(framebuffer, stride, x, 208, c, Graphics::encodeColor(0, 0, 255));
            x += 8;
        }
    }
    return 0;
}
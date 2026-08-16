#include "kernel.hh"
#include "Clock/Clock.hh"
#include "Console/Console.hh"
#include "Graphics/graphics.hh"
#include "Memory/Heap.hh"
#include "Memory/Framse.hh"
#include "Memory/Paging.hh"
#include "Text.hh"
#include "input/keyboard.hh"
#include "PCIe/pci.hh"
#include "PCIe/storage/Nvme.hh"
#include "shell/Shell.hh"
#include "Editor/Editor.hh"
#include "FileSystem/EnFS.hh"
#include "PCIe/storage/part.hh"


extern "C" UINT32 init_kernel(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GUID gopGuid = {0x9042a9de, 0x23dc, 0x4a38, 
        {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    SystemTable->BootServices->LocateProtocol(&gopGuid, 0, (void**)&gop);

    
    SystemTable->ConOut->Reset(SystemTable->ConOut, true);


    // Graphics    
    UINT32 bestMode = gop->Mode->Mode;
    UINT32 bestPixels = 0;

    for (UINT32 m = 0; m < gop->Mode->MaxMode; m++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *modeInfo = 0;
        UINTN infoSize = 0;

        if (gop->QueryMode(gop, m, &infoSize, &modeInfo) != 0) continue;
        if (modeInfo->PixelFormat > 1) continue;

        if (modeInfo->HorizontalResolution > 1920) continue;
        if (modeInfo->VerticalResolution > 1080) continue;

        
        UINT32 pixels = modeInfo->HorizontalResolution * modeInfo->VerticalResolution;

        if (pixels > bestPixels) {
            bestPixels = pixels;
            bestMode = m;
        }
    }
    if (bestMode != gop->Mode->Mode) gop->SetMode(gop, bestMode);

    UINT32 version = gop->Mode->Info->Version;
    UINT32 width = gop->Mode->Info->HorizontalResolution;
    UINT32 height = gop->Mode->Info->VerticalResolution;
    UINT32 stride = gop->Mode->Info->PixelsPerScanLine;
    UINT64 fbBase = gop->Mode->FrameBufferBase;
    UINT64 fbSize = gop->Mode->FrameBufferSize;

    Graphics::front = { (UINT32*)fbBase, width, height, stride};

    UINT32 *backPixels = 0;
    UINTN backBytes = (UINTN)stride * height * sizeof(UINT32);
    
    EFI_STATUS poolStatus = SystemTable->BootServices->AllocatePool(EfiLoaderData, backBytes, (void**)&backPixels);

    Graphics::back = { backPixels, width, height, stride};
    Graphics::back.clear(0x00000000);


    // Memory
    UINTN mapSize = 0;
    EFI_MEMORY_DESCRIPTOR *memoryMap = 0;
    UINTN mapKey;
    UINTN descriptorSize;
    UINT32 descriptorVersion;

    SystemTable->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);

    mapSize += 4 * descriptorSize;
    UINTN mapCapacity = mapSize;

    SystemTable->BootServices->AllocatePool(EfiLoaderData, mapCapacity, (void**)&memoryMap);

    EFI_STATUS MemoryStatus = SystemTable->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);

    UINT64 bitmapBytes = Frames::BitmapBytesNeeded(memoryMap, mapSize, descriptorSize);
    void *bitmapMemory = 0;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, bitmapBytes, &bitmapMemory);

    mapSize = mapCapacity;
    MemoryStatus = SystemTable->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);



    // EXIT UEFI

    EFI_STATUS BootStatus = SystemTable->BootServices->ExitBootServices(ImageHandle ,mapKey);

    Frames::Init(memoryMap, mapSize, descriptorSize, bitmapMemory, bitmapBytes);

    Paging::EnableWriteCombining();
    if (Paging::BuildKernelTables(memoryMap, mapSize, descriptorSize)) {
        Paging::Activate();
    }
    Paging::MapUc(fbBase, fbSize);
    Paging::MarkRangeWc(fbBase, fbSize);

    Heap::Init();
    Clock::StartClock();
    Pci::Scan();

    Text::DrawString(Graphics::back, 0, 0, "Waiting for NVME!");


    PciDevice *nvme = Pci::FindClass(0x01, 0X08);
    bool nvmeOk = Nvme::Init(nvme);
    bool idOk = nvmeOk && Nvme::IdentifyAll();
    bool ioOk = idOk && Nvme::CreateIoQueues();

    bool partOk = ioOk && Part::Mount(ENGINE_TYPE_GUID);
    EnFsResult fsResult = partOk ? EnFS::Mount() : ENFS_NO_PARTITION;

    if (!partOk) {
        Console::Println("no engine partition");
    } else if (fsResult != ENFS_OK) {
        Console::Print("enfs: ");
        Console::Println(EnFS::ResultName(fsResult));
    } else {
        Console::Print("enfs mounted, ");
        Console::PrintUInt(EnFS::super.fileCount);
        Console::Println(" files");
    }

    UINT64 lastTick = Clock::rdtsc();

    UINT64 lastFlash = 0;
    UINT64 flashDelay = 500;
    bool showCurrentLine = true;

    bool showPartTable = false;

    while (1) {
        Graphics::back.clear(0x00000000);
        UINT64 now = Clock::Millis();

        if (Editor::active) {
            Editor::Update(Keyboard::PollKey());
            Editor::Draw(Graphics::back);
        } else {
            char c = Keyboard::PollChar();

            if (c == '\n') {
                Shell::Execute(Console::CurrentLine());
            } else if (c != 0) {
                Console::Print(c);
            }

            Console::Draw(Graphics::back, 0, 216, 2);

            if (now - lastFlash >= flashDelay) {
                lastFlash = now;
                showCurrentLine = !showCurrentLine;
            }

            if (showCurrentLine) {
                // Currently has 2px down
                Text::DrawChar(Graphics::back, Console::col * 18, 200 + Console::lineCount * 18, '_', 2);
            }


            Text::DrawLogo(Graphics::back, 50, 50, 1);

            UINT64 nowTick = Clock::rdtsc();
            double dt = Clock::GetDeltaTime(lastTick, nowTick);
            lastTick = nowTick;

            double fps = (dt > 0.0) ? (1.0 / dt) : 0.0;

            CHAR16 fpsBuf[16];
            Text::UInt64ToStr(fps, fpsBuf);
            for (int i = 0; fpsBuf[i] != '\0'; i++) {
                Text::DrawChar(Graphics::back, i * 16, 0, (char)fpsBuf[i], 2);
            }

            CHAR16 debugInfo[16];
            Text::UInt64ToStr(width, debugInfo);
            for (int i = 0; debugInfo[i] != '\0'; i++){
                Text::DrawChar(Graphics::back, i * 16, 32, debugInfo[i], 2);
            }

            Text::DrawChar(Graphics::back, 80, 32, 'x', 2);
            Text::UInt64ToStr(height, debugInfo);
            for (int i = 0; debugInfo[i] != '\0'; i++) {
                Text::DrawChar(Graphics::back, i * 16 + 96, 32, debugInfo[i], 2);
            }

            Text::DrawUInt(Graphics::back, 80, 0, Clock::Millis());
        }


        Graphics::PresentFrame();
    }
    return 0;
}
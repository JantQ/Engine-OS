#include "../kernel.hh"
#include "../Clock/Clock.hh"
#include "../Graphics/graphics.hh"
#include "../Memory/Heap.hh"
#include "../Memory/Paging.hh"
#include "../Text.hh"
#include "../input/keyboard.hh"
#include "../Script/Script.hh"


extern "C" UINT8 gameAssets[GAME_HEADER_BYTES + SCRIPT_MAX_SOURCE] = {
    'E','n','G','i','N','e','G','a','M','e','B','l','O','b','1','!',
};


extern "C" UINT32 init_runtime(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GUID gopGuid = {0x9042a9de, 0x23dc, 0x4a38,
        {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    SystemTable->BootServices->LocateProtocol(&gopGuid, 0, (void**)&gop);

    SystemTable->ConOut->Reset(SystemTable->ConOut, true);

    UINT32 bestMode = gop->Mode->Mode;
    UINT32 bestPixles = 0;

    for (UINT32 m = 0; m < gop->Mode->MaxMode; m++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *modeInfo = 0;
        UINTN infoSize = 0;

        if (gop->QueryMode(gop, m, &infoSize, &modeInfo) != 0) continue;
        if (modeInfo->PixelFormat > 1) continue;

        if (modeInfo->HorizontalResolution > 1920) continue;
        if (modeInfo->VerticalResolution > 1080) continue;

        UINT32 pixles = modeInfo->HorizontalResolution * modeInfo->VerticalResolution;

        if (pixles > bestPixles) {
            bestPixles = pixles;
            bestMode = m;
        }
    }
    if (bestMode != gop->Mode->Mode) gop->SetMode(gop, bestMode);

    UINT32 width = gop->Mode->Info->HorizontalResolution;
    UINT32 height = gop->Mode->Info->VerticalResolution;
    UINT32 stride = gop->Mode->Info->PixelsPerScanLine;
    UINT64 fbBase = gop->Mode->FrameBufferBase;
    UINT64 fbSize = gop->Mode->FrameBufferSize;

    Graphics::front = { (UINT32*)fbBase, width, height, stride};

    UINT32 *backPixles = 0;
    UINTN backBytes = (UINTN)stride * height * sizeof(UINT32);

    SystemTable->BootServices->AllocatePool(EfiLoaderData, backBytes, (void**)&backPixles);

    Graphics::back = { backPixles, width, height, stride};
    Graphics::back.clear(0x00000000);


    UINTN mapSize = 0;
    EFI_MEMORY_DESCRIPTOR *memoryMap = 0;
    UINTN mapKey;
    UINTN descriptorSize;
    UINT32 descriptorVersion;

    SystemTable->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);

    mapSize += 2 * descriptorSize;

    SystemTable->BootServices->AllocatePool(EfiLoaderData, mapSize, (void**)&memoryMap);

    SystemTable->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);

    SystemTable->BootServices->ExitBootServices(ImageHandle, mapKey);

    Paging::EnableWriteCombining();
    Paging::MarkRangeWc(fbBase, fbSize);

    Heap::Init(memoryMap, mapSize, descriptorSize);
    Clock::StartClock();

    UINT64 gameLenght = 0;
    for (UINT32 i = 0; i < 8; i++) {
        gameLenght |= (UINT64)gameAssets[GAME_MAGIC_LENGHT + i] << (i * 8);
    }

    if (gameLenght > 0 && gameLenght <= SCRIPT_MAX_SOURCE) {
        INT32 badLine = Script::Load((const char*)(gameAssets + GAME_HEADER_BYTES), gameLenght);

        if (badLine < 0) {
            Script::RunLoop(false);

            while (1) {
                Graphics::back.clear(0x00000000);
                Text::DrawString(Graphics::back, width / 2 - 80, height / 2, "GAME ENDED", 2);
                Graphics::PresentFrame();
            }
        }

        while (1) {
            Graphics::back.clear(0x00000000);
            Text::DrawString(Graphics::back, width / 2 - 120, height / 2, "BAD SCRIPT LINE", 2);
            Text::DrawUInt(Graphics::back, width / 2 + 128, height / 2, (UINT64)badLine, 2);
            Graphics::PresentFrame();
        }
    }

    UINT64 lastTick = Clock::rdtsc();

    while (1) {
        Graphics::back.clear(0x00000000);

        Keyboard::PollChar();

        Text::DrawLogo(Graphics::back, width / 2 - 32, height / 2 - 96, 2);
        Text::DrawString(Graphics::back, width / 2 - 112, height / 2, "NO GAME LOADED", 2);

        UINT64 nowTick = Clock::rdtsc();
        double dt = Clock::GetDeltaTime(lastTick, nowTick);
        lastTick = nowTick;

        double fps = (dt > 0.0) ? (1.0 / dt) : 0.0;

        CHAR16 fpsBuf[16];
        Text::UInt64ToStr(fps, fpsBuf);
        for (int i = 0; fpsBuf[i] != '\0'; i++) {
            Text::DrawChar(Graphics::back, i * 16, 0, (char)fpsBuf[i], 2);
        }

        Graphics::PresentFrame();
    }
    return 0;
}

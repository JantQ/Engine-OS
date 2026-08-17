#include "Engine.hh"
#include "../input/keyboard.hh"
#include "../Graphics/graphics.hh"
#include "../Clock/Clock.hh"
#include "../Text.hh"
#include "../Memory/Framse.hh"
#include "../Script/Script.hh"

extern "C" INT64 EnKeyDown(INT64 code) {
    Keyboard::PumpState();
    return Keyboard::IsDown((UINT16)code) ? 1 : 0;
}

extern "C" INT64 EnScreenW() {
    return (INT64)Graphics::back.width;
}

extern "C" INT64 EnScreenH() {
    return (INT64)Graphics::back.height;
}

extern "C" INT64 EnMillis() {
    return (INT64)Clock::Millis();
}

extern "C" void EnWait(INT64 ms) {
    if (ms <= 0) return;
    UINT64 until = Clock::Millis() + (UINT64)ms;
    while (Clock::Millis() < until) Keyboard::PumpState();
}

extern "C" void EnRect(EnCtx *ctx) {
    Graphics::DrawRectangle(Graphics::back, (INT32)ctx->args[0], (INT32)ctx->args[1], (INT32)ctx->args[2], (INT32)ctx->args[3], (INT32)ctx->args[4]);
}

extern "C" void EnText(EnCtx *ctx) {
    Text::DrawString(Graphics::back, (UINT32)ctx->args[0], (UINT32)ctx->args[1], ctx->strings + ctx->args[2], 2);
}

extern "C" void EnNum(EnCtx *ctx) {
    Text::DrawUInt(Graphics::back, (UINT32)ctx->args[0], (UINT32)ctx->args[1], (UINT64)ctx->args[2], 2);
}

extern "C" INT64 EnFlip(EnCtx *ctx) {
    Graphics::PresentFrame();
    Graphics::back.clear(0x00000000);
    Keyboard::PumpState();
    ctx->fuel = 200000;
    return Keyboard::IsDown(KEY_ESC) ? 0 : 1;
}

extern "C" INT64 EnAlloc(EnCtx *ctx) {
    if (!ctx->arena) {
        ctx->arena = (INT64*)Frames::Alloc(SCRIPT_ARENA_BYTES / FRAME_SIZE);
        ctx->arenaSlots = ctx->arena ? SCRIPT_ARENA_BYTES / 8 : 0;
        ctx->arenaUsed = 1;
    }

    INT64 amount = ctx->args[0];
    if (amount < 1) amount = 1;

    if (!ctx->arena || ctx->arenaUsed + (UINT64)amount > ctx->arenaSlots) return -1;

    INT64 base = (INT64)ctx->arenaUsed;
    ctx->arenaUsed += (UINT64)amount;
    return base;
}

extern "C" void EnPoke(EnCtx *ctx) {
    INT64 slot = ctx->args[0] + ctx->args[1];
    if (ctx->arena && slot >= 1 && (UINT64)slot < ctx->arenaUsed) {
        ctx->arena[slot] = ctx->args[2];
    }
}

extern "C" INT64 EnPeek(EnCtx *ctx) {
    INT64 slot = ctx->args[0] + ctx->args[1];
    if (ctx->arena && slot >= 1 && (UINT64)slot < ctx->arenaUsed) {
        return ctx->arena[slot];
    }
    return 0;
}
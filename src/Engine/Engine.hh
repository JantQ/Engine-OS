#pragma once
#include "../kernel.hh"
#include "../Compiler/Compiler.hh"

extern "C" INT64 EnKeyDown(INT64 code);
extern "C" INT64 EnScreenW();
extern "C" INT64 EnScreenH();
extern "C" INT64 EnMillis();
extern "C" void EnWait(INT64 ms);
extern "C" void EnRect(EnCtx *ctx);
extern "C" void EnText(EnCtx *ctx);
extern "C" void EnNum(EnCtx *ctx);
extern "C" INT64 EnFlip(EnCtx *ctx);
extern "C" INT64 EnAlloc(EnCtx *ctx);
extern "C" void EnPoke(EnCtx *ctx);
extern "C" INT64 EnPeek(EnCtx *ctx);
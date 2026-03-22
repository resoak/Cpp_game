#pragma once
// ================================================================
//  font.h — 中文字型載入、FS_* 常數、文字繪製工具
// ================================================================
#include "raylib.h"

// ── 字型全域（font.cpp 中定義）──────────────────────────────────
extern Font gFont;
extern bool gFontLoaded;

void LoadChineseFont();

// ── 字型大小常數 ─────────────────────────────────────────────────
constexpr float FS_TINY  = 20.f;
constexpr float FS_SMALL = 24.f;
constexpr float FS_MED   = 30.f;
constexpr float FS_LARGE = 36.f;
constexpr float FS_TITLE = 44.f;
constexpr float FS_BIG   = 60.f;

// ── 文字繪製工具（inline，避免 call overhead）────────────────────
inline float MCN(const char* t, float sz) {
    return MeasureTextEx(gFont, t, sz, 0).x;
}
inline void DTX(const char* t, float x, float y, float sz, Color c) {
    DrawTextEx(gFont, t, {x, y}, sz, 0, c);
}
inline void DTC(const char* t, int x, int y, float sz, Color c) {
    DTX(t, (float)x - MCN(t, sz) * 0.5f, (float)y - sz * 0.5f, sz, c);
}

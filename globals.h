#pragma once
// ================================================================
//  globals.h — 螢幕縮放、震動偏移全域變數
// ================================================================
#include "raylib.h"

// ── 縮放與偏移（globals.cpp 中定義）────────────────────────────
extern float gScale;
extern float gOffX;
extern float gOffY;
extern float gShkX;
extern float gShkY;

// ── 虛擬滑鼠座標 ─────────────────────────────────────────────────
inline Vector2 VirtualMouse() {
    Vector2 m = GetMousePosition();
    return { (m.x - gOffX) / gScale, (m.y - gOffY) / gScale };
}

#pragma once
// ================================================================
//  path.h — 路徑格子、預設路線、全域路徑資料
// ================================================================
#include "constants.h"
#include <vector>

// ── 路徑基礎結構 ─────────────────────────────────────────────────
struct PathCell { int gx, gy; };

struct PathPreset {
    int         count;
    int         wx[16];
    int         wy[16];
    const char* name;
};

// ── 5 條預設路線（定義在 path.cpp）──────────────────────────────
extern const PathPreset PATH_PRESETS[5];

// ── 當前路徑狀態（定義在 path.cpp）─────────────────────────────
extern int                   CURRENT_PATH_IDX;
extern int                   CURRENT_PATH_IDX2;
extern const PathPreset*     CUR_PRESET;
extern const PathPreset*     CUR_PRESET2;
extern std::vector<PathCell> PATH_CELLS;
extern std::vector<PathCell> PATH_CELLS2;
extern bool IS_PATH [24][20];
extern bool IS_PATH2[24][20];

// ── 建置函式 ─────────────────────────────────────────────────────
void BuildPath(int presetIdx = 0);
void BuildDualPath(int presetIdx = 1);

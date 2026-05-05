#pragma once
// ================================================================
//  path.h — 路徑格子、預設路線、全域路徑資料
// ================================================================
#include "constants.h"
#include <array>
#include <vector>

constexpr int ENTRY_SIDE_COUNT = 4;

enum class PathEntrySide {
    LEFT,
    TOP,
    RIGHT,
    BOTTOM,
};

// ── 路徑基礎結構 ─────────────────────────────────────────────────
struct PathCell { int gx, gy; };

struct PathPreset {
    int         count;
    int         wx[16];
    int         wy[16];
    const char* name;
    PathEntrySide entrySide;
    int         family;
    int         dualWeight;
};

// ── 作者預設路線（定義在 path.cpp）──────────────────────────────
extern const PathPreset PATH_PRESETS[];
extern const int        PATH_PRESET_COUNT;

// ── 當前路徑狀態（定義在 path.cpp）─────────────────────────────
extern std::array<int, MAX_LANES>                   CURRENT_PATH_IDX;
extern std::array<const PathPreset*, MAX_LANES>     CUR_PRESET;
extern std::array<std::vector<PathCell>, MAX_LANES> PATH_CELLS;
extern bool IS_PATH[MAX_LANES][24][20];

int  NormalizePathPresetIdx(int presetIdx);
const PathPreset&              GetPathPreset(int presetIdx);
int                            GetActiveLanePresetIdx(int laneSlot);
const PathPreset&              GetActiveLanePreset(int laneSlot);
const std::vector<PathCell>&   GetActiveLaneCells(int laneSlot);
bool                           IsActiveLaneCell(int laneSlot, int gx, int gy);
bool                           IsAnyActivePathCell(int gx, int gy, int activeLaneCount = 1);
std::vector<PathCell>          BuildPresetPathCells(int presetIdx);
void                           SetActiveLanePreset(int laneSlot, int presetIdx);

// ── 建置函式 ─────────────────────────────────────────────────────
void BuildPath(int presetIdx = 0);
void BuildDualPath(int presetIdx = 1);
